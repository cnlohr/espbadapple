"""
Jointly trains patches and sequence as parameters.

This allows fully arbitrary sequences and patch sets, with no particular restrictions on tiles being separated least-
squares matches of target video patches. However, since we are learning a one-hot tile index for every patch of every
frame, there are several orders of magnitude more parameters to be learned.
"""

import torch
import torch.nn as nn
import torchvision.utils
from ttools.modules.losses import LPIPS
from video_data import VideoFrames
from torch.utils.data import DataLoader
import torch.nn.functional as F
import math
import matplotlib.pyplot as plt
import os
import time
import numpy as np
import cv2
from scipy.ndimage.filters import gaussian_filter
from blocksettings import *

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")


class ImageReconstruction(nn.Module):
    def __init__(self, n_sequence):
        super().__init__()
        self.n_blocks = nblocks
        self.n_sequence = n_sequence

        self.tiles_per_img = img_size[0] * img_size[1] // block_size[0] // block_size[1]

        # For assembling an image out of chosen blocks
        self.fold = torch.nn.Fold(output_size=img_size,
                                  kernel_size=block_size,
                                  stride=block_size)

        # Blocks for reconstruction
        self.blocks = None

        # Sequence (one hot per image tile per frame)
        # Note this parameter will be large
        self.sequence = None

    def init_blocks_tiledata(self, tiles_path):
        tiles = np.fromfile(tiles_path, dtype=np.float32)
        tiles = tiles.reshape(-1, block_size[0], block_size[1])
        tiles = tiles[:self.n_blocks, ...]

        self.blocks = nn.Parameter(torch.Tensor(tiles), requires_grad=True)

    def init_sequence(self, sequence_path):
        s = np.fromfile(sequence_path, dtype=np.int32)
        s = s.reshape(-1, self.tiles_per_img).astype(np.int64)
        st = torch.from_numpy(s)

        # sequence is a categorical distribution. Choose logits that put a lot of weight on the initial class
        scale = np.sqrt(self.n_blocks / 2)
        ofs = scale / 2
        self.sequence = nn.Parameter(nn.functional.one_hot(st, num_classes=self.n_blocks).float() * scale - ofs,
                                     requires_grad=True)

    def self_similarity_regularization(self):
        fb = self.blocks.view(self.n_blocks, -1)

        # output shape [n_blocks, n_blocks, -1] where [i, j, :] is the squared difference between blocks i and j
        diffs = torch.square(fb[:, None, :] - fb[None, :, :])

        # negative -> we want to encourage separation
        return -1 * torch.mean(diffs)

    def tile_f2f_penalty(self, frame_idxs):
        """
        A regularization that penalizes changes in tiles between frames. This represents a quality desirable for video
        compression - if tiles change less, we can compress the video more.

        Mathematically, this term wants to minimize the KL-divergence between a given tile's categorical distributions
        between frames i and i+1. This allows gradients for a given pair of tiles/frames to flow into all blocks.

        Note that this penalty is directly against the concept of a movie, as in "moving picture." If you give this loss
        too much weight, you might end up with a slideshow or a single static image.
        """
        frame_idxs_b = torch.clip(frame_idxs+1, min=None, max=self.sequence.shape[0]-1)

        logprob_a = F.log_softmax(self.sequence[frame_idxs, ...])
        logprob_b = F.log_softmax(self.sequence[frame_idxs_b, ...])

        kl_div_elementwise = F.kl_div(logprob_a, logprob_b, reduction='none', log_target=True)
        kl_div = kl_div_elementwise.sum(-1)  # Sum over categorical dim to yield per-frame/per-tile KL divergence

        return kl_div.mean()

    def forward(self, frame_idxs):
        """
        Index into stored sequence weights; use these to assemble a batch of images.

        :param frame_idxs: Batch of frame indices to consider.
        :return: An image reconstructed by combining input blocks.
        """
        block_len = math.prod(block_size)

        # Pluck out selected block weights (one hot per image tile)
        if self.training:
            # Use the gumbel-softmax trick to sample our tiles
            block_weights = F.gumbel_softmax(logits=self.sequence[frame_idxs, ...],
                                             tau=0.5,  # TODO tune me
                                             hard=True)
        else:
            # Use argmax
            block_weights = F.one_hot(self.sequence[frame_idxs, ...].argmax(dim=-1),
                                      num_classes=self.sequence.size(-1)
                                      ).float()

        # recreate (unfolded) target image from stored sequence
        uf_reconstructed = torch.matmul(block_weights, self.blocks.view(self.n_blocks, block_len)).permute(0, 2, 1)

        # fold back into expected image shape
        reconstructed = self.fold(uf_reconstructed)

        return reconstructed

    def dump_grid(self, out_path):
        grid = torchvision.utils.make_grid(self.blocks[:, None, ...], nrow=16, padding=2)
        torchvision.utils.save_image(grid, out_path)

    def dump_blocks(self, out_path):
        # Write blocks back into the same format as read by init_blocks_tiledata
        tiles = self.blocks.detach().cpu().numpy().astype(np.float32)
        tiles.tofile(out_path)

    def dump_sequence(self, out_path):
        # write argmax'd indicies back into the same format as read by init_sequence
        indices = self.sequence.argmax(dim=-1)
        indices_np = indices.detach().cpu().numpy().astype(np.int32)
        indices_np.tofile(out_path)


class BlockTrainer:
    def __init__(self):

        self.dataset = VideoFrames(device)
        self.dataset.load_data("Touhou - Bad Apple.mp4")

        self.recr = ImageReconstruction(n_sequence=len(self.dataset))

        self.recr.init_blocks_tiledata("kmeans_256_ni_mse.dat")
        self.recr.init_sequence("stream-kmeans_256_ni.dat")
        self.recr.to(device)

        self.data_loader = DataLoader(self.dataset,
                                      batch_size=32,
                                      shuffle=True)

        self.optim = torch.optim.Adam(self.recr.parameters(), lr=0.002)

        self.perceptual_loss = LPIPS().to(device)

        tstr = time.strftime("%Y-%m-%d_%H-%M-%S")
        self.out_dir = os.path.join("outputs", "ts256_ni_" + tstr)
        self.out_blocks_dir = os.path.join(self.out_dir, "blocks")
        self.out_img_dir = os.path.join(self.out_dir, "imgs")
        self.out_data_dir = os.path.join(self.out_dir, "data")

        # Weighting factor for the tile-change regularization
        self.change_lambda = 0.01

        os.makedirs(self.out_data_dir, exist_ok=False)
        os.makedirs(self.out_blocks_dir, exist_ok=False)
        os.makedirs(self.out_img_dir, exist_ok=False)

    def dump_reconstructed_frame(self, out_path, frame_n=1141):
        with torch.no_grad():
            idxs = [self.dataset[frame_n][1]]
            recr_frame = self.recr(idxs)
            torchvision.utils.save_image(recr_frame, out_path)

    def train(self):
        self.recr.dump_grid(os.path.join(self.out_blocks_dir, "000_init_blocks.png"))

        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1141.png"), 1141)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_0080.png"), 80)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_2303.png"), 2303)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1659.png"), 1659)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1689.png"), 1689)

        for epoch in range(1000000):
            self.recr.train()

            epoch_loss = 0.0
            epoch_loss_percep = 0.0
            epoch_loss_change = 0.0

            epoch_n = 0

            for batch_n, (target_img, idx) in enumerate(self.data_loader, start=0):
                self.optim.zero_grad()

                reconstructed_img = self.recr(idx)

                # upsample reconstructed and target images to match vgg trained image size (well, width at least)
                tgt_us = nn.functional.interpolate(target_img, size=(192, 256), mode='nearest')
                rcd_us = nn.functional.interpolate(reconstructed_img, size=(192, 256), mode='nearest')

                loss_percep = self.perceptual_loss(rcd_us, tgt_us)
                loss_change = self.recr.tile_f2f_penalty(idx)

                loss = loss_percep + self.change_lambda * loss_change

                loss.backward()
                self.optim.step()

                # clamp block weight range
                self.recr.blocks.data.clamp_(0.0, 1.0)

                epoch_loss += loss.item()
                epoch_loss_percep += loss_percep.item()
                epoch_loss_change += loss_change.item()

                epoch_n += 1

                if epoch < 3:
                    print("Epoch %d Batch %d: Loss %f (percep %f choice %f)" % (epoch, batch_n, loss.item(), loss_percep.item(), loss_change.item()))

            epoch_loss /= epoch_n
            epoch_loss_percep /= epoch_n
            epoch_loss_change /= epoch_n

            self.recr.eval()

            # write binary video data
            self.recr.dump_blocks(os.path.join(self.out_data_dir, "%d_%0.06f_p%0.06f_c%0.06f_blocks.dat" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change)))
            self.recr.dump_sequence(os.path.join(self.out_data_dir, "%d_%0.06f_p%0.06f_c%0.06f_stream.dat" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change)))

            # write visualization for humans
            self.recr.dump_grid(os.path.join(self.out_blocks_dir, "%d_%0.06f_blocks.png" % (epoch, epoch_loss)))

            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%04d_%0.06f_img_1141.png" % (epoch, epoch_loss)), 1141)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%04d_%0.06f_img_0080.png" % (epoch, epoch_loss)), 80)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%04d_%0.06f_img_2303.png" % (epoch, epoch_loss)), 2303)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%04d_%0.06f_img_1659.png" % (epoch, epoch_loss)), 1659)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%04d_%0.06f_img_1689.png" % (epoch, epoch_loss)), 1689)

            print("Epoch %d: loss %f (percep %f change %f)" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change))


def main():
    trainer = BlockTrainer()
    trainer.train()


if __name__ == "__main__":
    main()
