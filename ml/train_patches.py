import sys
import torch
import torch.nn as nn
import torchvision.utils
from ttools.modules.losses import LPIPS
from video_data import VideoFrames
from torch.utils.data import DataLoader
import math
import os
import time
import numpy as np
import cv2
from scipy.ndimage.filters import gaussian_filter
from blocksettings import *

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

class ImageReconstruction(nn.Module):
    def __init__(self):
        super().__init__()
        self.n_blocks = 256

        self.tiles_per_img = img_size[0] * img_size[1] / block_size[0] / block_size[1]

        # unfold/fold for image reconstruction
        self.unfold = torch.nn.Unfold(kernel_size=block_size,
                                      stride=block_size)
        self.fold = torch.nn.Fold(output_size=img_size,
                                  kernel_size=block_size,
                                  stride=block_size)

        # Blocks for reconstruction
        self.blocks = None

        # Support inverted blocks in the bitstream?
        self.invert_blocks = False

    def init_blocks_haar(self):
        """
        Initialize blocks as something vaguely resembling Haar cascades.
        Good for getting an orthogonal-ish initial set.
        """
        # Init blocks as something vaguely resembling Haar cascades
        basis = np.array([
            [0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 1, 1, 1, 1],
            [0, 0, 1, 1, 1, 1, 0, 0],
            [0, 1, 1, 1, 1, 0, 0, 0],
            [0, 0, 0, 1, 1, 1, 1, 0],
            [0, 0, 1, 1, 0, 0, 1, 1],
            [0, 1, 1, 0, 0, 1, 1, 0],
            # [0, 1, 0, 1, 0, 1, 0, 1]  # disabled: blur below

        ], dtype=np.bool_)

        out = []
        out_tuple = []

        for i in range(basis.shape[0]):
            for j in range(basis.shape[0]):
                for ai in range(2):
                    a = basis[i]
                    if ai % 2 == 1:
                        a = np.logical_not(a)

                    for bi in range(2):
                        b = basis[j]
                        if bi % 2 == 1:
                            b = np.logical_not(b)

                        candidates = list()

                        candidates.append(a.reshape(1, 8) ^ b.reshape(8, 1))
                        candidates.append(candidates[-1].T)
                        candidates.append(np.logical_not(candidates[-1]))
                        candidates.append(candidates[-1].T)
                        candidates.append(np.logical_and(a.reshape(1, 8), b.reshape(8, 1)))
                        candidates.append(candidates[-1].T)
                        candidates.append(np.logical_not(candidates[-1]))
                        candidates.append(candidates[-1].T)
                        candidates.append(np.logical_or(a.reshape(1, 8), b.reshape(8, 1)))
                        candidates.append(candidates[-1].T)
                        candidates.append(np.logical_not(candidates[-1]))
                        candidates.append(candidates[-1].T)

                        for c in candidates:
                            reps = [tuple(map(tuple, c))]

                            # check for inversions (and other potentially-dynamically-created tiles)
                            if self.invert_blocks:
                                reps.append(tuple(map(tuple, np.logical_not(c))))

                            insert_ok = True
                            for rep in reps:
                                if rep in out_tuple:
                                    insert_ok = False

                            if insert_ok:
                                out_tuple.append(reps[0])
                                out.append(c)

        out = out[:self.n_blocks]

        # blur these a little to make gradients more likely
        for i in range(len(out)):
            out[i] = gaussian_filter(out[i].astype(np.float32), sigma=np.sqrt(2))

        out = np.array(out, dtype=np.float32)
        self.blocks = nn.Parameter(torch.Tensor(out), requires_grad=True)

    def init_blocks_img(self, img_path):
        """
        Initialize blocks by reading one of the tileset images output by previous training.
        """
        grid = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)

        blocks_raw = []
        for i in range(16):
            for j in range(16):
                bx = 2 + (block_size+2) * i
                by = 2 + (block_size+2) * j
                b = grid[bx:bx + block_size, by:by + block_size].astype(float) / 255.0
                blocks_raw.append(b)

        self.blocks = nn.Parameter(torch.Tensor(blocks_raw), requires_grad=True)

    def init_blocks_tiledata(self, tiles_path):
        """
        Initialize blocks using binary tile data from/for the C side.
        """
        tiles = np.fromfile(tiles_path, dtype=np.float32)
        tiles = tiles.reshape(-1, block_size[0], block_size[1])
        tiles = tiles[:self.n_blocks, ...]

        self.blocks = nn.Parameter(torch.Tensor(tiles), requires_grad=True)

    def forward(self, target_img):
        """
        Using elements of self.blocks, reconstruct target_img by finding best per-block matches.

        This operation is fully differentiable, but:
            - uses a simple loss for comparing blocks against image patches
            - uses a (sharpened) soft-argmin to compose a reconstructed target, which may lead to some mixing (until
              blocks separate themselves, at least).

        :param target_img: The full image to reconstruct.
        :return: An image reconstructed by combining input blocks.
        """
        # Sharpening factor applied to block matches.
        # Bigger = less linear mixing in the output patches; smaller = gradients spread across more candidates.
        beta = 35

        block_len = math.prod(block_size)

        uf_tgt = self.unfold(target_img)

        if self.invert_blocks:
            # Expand our block set to include inverted versions of our tiles
            n_blocks = self.n_blocks * 2
            block_cd = torch.zeros((n_blocks, *block_size), dtype=torch.float32, device=self.blocks.get_device())
            block_cd[:self.n_blocks, ...] = self.blocks
            block_cd[self.n_blocks:, ...] = 1 - self.blocks
        else:
            n_blocks = self.n_blocks
            block_cd = self.blocks

        # Compare our blocks against each extracted block in uf_tgt
        a = uf_tgt.view(-1, 1, block_len, uf_tgt.shape[-1])
        b = block_cd.view(1, n_blocks, block_len, 1)
        sqerr = torch.sum(torch.square(a - b), dim=2)

        # (sharpened) softmax over block-choice dim
        block_weights = torch.softmax(-1 * sqerr * beta, dim=1)

        # recreate (unfolded) target by weighted products of self.blocks and block_weights
        uf_reconstructed = torch.matmul(block_weights.permute(0, 2, 1),
                                        block_cd.view(n_blocks, block_len)).permute(0, 2, 1)

        # fold back into expected image shape
        reconstructed = self.fold(uf_reconstructed)

        # In an ideal world, for each block there is a single best batch and no similar candidates.
        # Explicitly penalize the above creating linear combinations of blocks.
        choice_penalty = torch.mean(torch.minimum(block_weights, 1 - block_weights)) * self.n_blocks

        return reconstructed, choice_penalty

    def dump_grid(self, out_path):
        """
        Output grid as images.
        """
        grid = torchvision.utils.make_grid(self.blocks[:, None, ...], nrow=16, padding=2)
        torchvision.utils.save_image(grid, out_path)


class BlockTrainer:
    def __init__(self):

        self.dataset = VideoFrames(device)
        self.dataset.load_data("Touhou - Bad Apple.mp4")

        self.recr = ImageReconstruction()

        # self.recr.init_blocks_haar()
        self.recr.init_blocks_tiledata("kmeans_ni_mse.dat")

        self.recr.to(device)

        self.data_loader = DataLoader(self.dataset,
                                      batch_size=48,
                                      shuffle=True)

        self.optim = torch.optim.Adam(self.recr.parameters(), lr=0.002)

        self.perceptual_loss = LPIPS().to(device)

        tstr = time.strftime("%Y-%m-%d_%H-%M-%S")
        self.out_dir = os.path.join("outputs",
                                    ("ts%d_" % self.recr.n_blocks) + ("i_" if self.recr.invert_blocks else "") + tstr)
        self.out_blocks_dir = os.path.join(self.out_dir, "blocks")
        self.out_img_dir = os.path.join(self.out_dir, "imgs")

        os.makedirs(self.out_blocks_dir, exist_ok=False)
        os.makedirs(self.out_img_dir, exist_ok=False)

    def dump_reconstructed_frame(self, out_path, frame_n=1141):
        with torch.no_grad():
            frame = self.dataset[frame_n][0][None, ...]
            recr_frame, _ = self.recr(frame)
            torchvision.utils.save_image(recr_frame, out_path)

    def train(self):
        self.recr.dump_grid(os.path.join(self.out_blocks_dir, "000_init_blocks.png"))
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1141.png"), 1141)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_0080.png"), 80)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_2303.png"), 2303)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1659.png"), 1659)
        self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "000_init_img_1689.png"), 1689)

        for epoch in range(1000000):

            epoch_loss = 0.0
            epoch_loss_p = 0.0
            epoch_loss_c = 0.0

            epoch_n = 0

            for batch_n, (target_img, _) in enumerate(self.data_loader, start=0):
                self.optim.zero_grad()

                reconstructed_img, choice_penalty = self.recr(target_img)

                # upsample reconstructed and target images to match vgg trained image width
                tgt_us = nn.functional.interpolate(target_img, size=(img_size[0]*2, img_size[1]*2), mode='bilinear', align_corners=False)
                rcd_us = nn.functional.interpolate(reconstructed_img, size=(img_size[0]*2, img_size[1]*2), mode='bilinear', align_corners=False)

                loss_p = self.perceptual_loss(rcd_us, tgt_us)
                loss_c = choice_penalty

                loss = loss_p + loss_c

                loss.backward()
                self.optim.step()

                # clamp parameter range
                for p in self.recr.parameters():
                    p.data.clamp_(0.0, 1.0)

                epoch_loss += loss.item()
                epoch_loss_p += loss_p.item()
                epoch_loss_c += loss_c.item()

                epoch_n += 1

                if epoch < 3:
                    print("Epoch %d Batch %d: Loss %f (percep %f choice %f)" % (epoch, batch_n, loss.item(), loss_p.item(), loss_c.item()))

            epoch_loss /= epoch_n
            epoch_loss_p /= epoch_n
            epoch_loss_c /= epoch_n

            self.recr.dump_grid(os.path.join(self.out_blocks_dir, "%d_%0.06f_blocks.png" % (epoch, epoch_loss)))
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%d_%0.06f_img_1141.png" % (epoch, epoch_loss)), 1141)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%d_%0.06f_img_0080.png" % (epoch, epoch_loss)), 80)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%d_%0.06f_img_2303.png" % (epoch, epoch_loss)), 2303)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%d_%0.06f_img_1659.png" % (epoch, epoch_loss)), 1659)
            self.dump_reconstructed_frame(os.path.join(self.out_img_dir, "%d_%0.06f_img_1689.png" % (epoch, epoch_loss)), 1689)

            print("Epoch %d: loss %f (percep %f choice %f)" % (epoch, epoch_loss, epoch_loss_p, epoch_loss_c))

            sys.stdout.flush()

def main():
    trainer = BlockTrainer()
    trainer.train()


if __name__ == "__main__":
    main()
