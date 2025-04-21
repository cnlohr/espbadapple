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
from video_data import VideoFrames, ContigChunkSampler
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

import torch

def trilinear_interp(lut: torch.Tensor, x: torch.Tensor, y: torch.Tensor, z: torch.Tensor) -> torch.Tensor:
    """
    LUT: (Dx, Dy, Dz)
    x, y, z: same shape S, floats in [0, D?-1]
    returns v: shape S
    """
    Dx, Dy, Dz = lut.shape

    # neighbor indices
    x0 = x.floor().long().clamp(0, Dx - 2)
    y0 = y.floor().long().clamp(0, Dy - 2)
    z0 = z.floor().long().clamp(0, Dz - 2)
    x1 = (x0 + 1).clamp(0, Dx - 1)
    y1 = (y0 + 1).clamp(0, Dy - 1)
    z1 = (z0 + 1).clamp(0, Dz - 1)

    # interp weights
    xd = x - x0.float()
    yd = y - y0.float()
    zd = z - z0.float()

    # cube corner values
    c000 = lut[x0, y0, z0]
    c100 = lut[x1, y0, z0]
    c010 = lut[x0, y1, z0]
    c110 = lut[x1, y1, z0]
    c001 = lut[x0, y0, z1]
    c101 = lut[x1, y0, z1]
    c011 = lut[x0, y1, z1]
    c111 = lut[x1, y1, z1]

    # interpolate in xy planes at z0 and z1
    c00 = c000 * (1 - xd) * (1 - yd) \
        + c100 * xd       * (1 - yd) \
        + c010 * (1 - xd) * yd       \
        + c110 * xd       * yd

    c01 = c001 * (1 - xd) * (1 - yd) \
        + c101 * xd       * (1 - yd) \
        + c011 * (1 - xd) * yd       \
        + c111 * xd       * yd

    # final interp along z
    v = c00 * (1 - zd) + c01 * zd

    return v

def gen_deblocking_lut():
    # 3D LUT for deblocking op
    # The actual math on the microcontroller is integer and bitwise ops that are hostile to gradients
    # Here we make a LUT of the operation, which is exact for integer input and can be interpolated for valid gradients
    # Addressing: (center, left, right) -> (output)
    lut = torch.zeros((3, 3, 3), dtype=torch.float32, device=device)
    lut[0, 0, 0] = 0.0
    lut[1, 0, 0] = 0.0
    lut[2, 0, 0] = 1.0
    lut[0, 0, 1] = 0.0
    lut[1, 0, 1] = 1.0
    lut[2, 0, 1] = 2.0
    lut[0, 0, 2] = 1.0
    lut[1, 0, 2] = 1.0
    lut[2, 0, 2] = 2.0
    lut[0, 1, 0] = 0.0
    lut[1, 1, 0] = 1.0
    lut[2, 1, 0] = 2.0
    lut[0, 1, 1] = 1.0
    lut[1, 1, 1] = 1.0
    lut[2, 1, 1] = 2.0
    lut[0, 1, 2] = 1.0
    lut[1, 1, 2] = 2.0
    lut[2, 1, 2] = 2.0
    lut[0, 2, 0] = 1.0
    lut[1, 2, 0] = 1.0
    lut[2, 2, 0] = 2.0
    lut[0, 2, 1] = 1.0
    lut[1, 2, 1] = 2.0
    lut[2, 2, 1] = 2.0
    lut[0, 2, 2] = 1.0
    lut[1, 2, 2] = 2.0
    lut[2, 2, 2] = 2.0

    return lut


def deblocking_filter(img_raw, lut, quantize=False):
    """
    Runs Charles' custom deblocking filter on inputs, optionally with quantization.
    """

    # Range scaling to [0..2]
    img = img_raw * 2

    B, C, H, W = img.shape

    # Create 1D masks for input coordinates, marking where the block filter would apply
    x_indices = torch.arange(W, device=img.device)
    y_indices = torch.arange(H, device=img.device)
    mask_x = (x_indices % block_size[1] == 0) | (x_indices % block_size[1] == (block_size[1] - 1))
    mask_y = (y_indices % block_size[0] == 0) | (y_indices % block_size[0] == (block_size[0] - 1))

    # Reshape for broadcasting
    mask_x = mask_x.view(1, 1, 1, W)
    mask_y = mask_y.view(1, 1, H, 1)

    # left/right filter is evaluated first
    padded = F.pad(img, (1, 1, 1, 1), mode='replicate')  # Pad w/ replication to match c impl
    center = padded[:, :, 1:-1, 1:-1]
    left = padded[:, :, 1:-1, :-2]
    right = padded[:, :, 1:-1, 2:]
    filtered = torch.where(mask_x, trilinear_interp(lut, center, left, right), center)

    if quantize:
        filtered = filtered + (torch.round(filtered) - filtered).detach()

    # up/down filter is evaluated second
    padded2 = F.pad(filtered, (1, 1, 1, 1), mode='replicate')
    center2 = padded2[:, :, 1:-1, 1:-1]
    up = padded2[:, :, 1:-1, :-2]
    down = padded2[:, :, 1:-1, 2:]
    filtered2 = torch.where(mask_y, trilinear_interp(lut, center, up, down), center2)

    if quantize:
        filtered2 = filtered2 + (torch.round(filtered2) - filtered2).detach()

    # Undo scaling
    filtered2 /= 2

    return filtered2


class TileMSEMatcher(nn.Module):
    def __init__(self):
        super().__init__()
        self.unfold = torch.nn.Unfold(kernel_size=block_size,
                                      stride=block_size)

    def forward(self, blocks, target_img):
        block_len = math.prod(block_size)
        uf_tgt = self.unfold(target_img)

        # compare blocks against each extracted block in uf_tgt
        a = uf_tgt.view(-1, 1, block_len, uf_tgt.shape[-1])
        b = blocks.view(1, nblocks, block_len, 1)
        mse = torch.mean(torch.square(a - b), dim=2).permute(0, 2, 1)  # reorder to: (B, tiles_per_img, nblocks)

        return mse


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
        self.train_sequence = True

        # Lookup table for deblocking filter
        self.lut = gen_deblocking_lut()

        # Gumbel-Softmax temperature. Note this term is annealed by the training loop
        self.gs_tau = 1.0


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

    def init_sequence_matching(self, data):
        """
        Initialize sequence probabilities by matching blocks against tiles from the video.
        Per-tile categorical distributions are initialized based on MSE losses.
        The idea is to assign initial probabilities to blocks based on how well they match a given tile.
        This operation is computationally expensive, so we express it as a convolution and accelerate it on the GPU.
        """
        with torch.no_grad():
            matcher = TileMSEMatcher()
            dl = DataLoader(dataset=data,
                            batch_size=32,
                            shuffle=False)

            self.sequence = nn.Parameter(torch.zeros((len(data), self.tiles_per_img, nblocks), dtype=torch.float32, device=self.blocks.device), requires_grad=True)

            tau = 0.002  # temperature parameter, controls "peakiness" of resulting distribution

            for imgs, idxs in dl:
                mse = matcher(self.blocks, imgs)
                self.sequence[idxs] = -mse/tau

    def self_similarity_regularization(self):
        fb = self.blocks.view(self.n_blocks, -1)

        # output shape [n_blocks, n_blocks, -1] where [i, j, :] is the squared difference between blocks i and j
        diffs = torch.square(fb[:, None, :] - fb[None, :, :])

        # negative -> we want to encourage separation
        return -1 * torch.mean(diffs)

    def tile_f2f_penalty(self, frame_idxs_in):
        """
        A regularization that penalizes changes in tiles between frames. This represents a quality desirable for video
        compression - if tiles change less, we can compress the video more.

        Note that this penalty is directly against the concept of a movie, as in "moving picture." If you give this loss
        too much weight, you might end up with a slideshow or a single static image.
        """

        # this only works if we have more than two frames in a batch
        if len(frame_idxs_in) < 2:
            return torch.tensor([0.0], dtype=torch.float32, device=device)

        frame_idxs = frame_idxs_in[:-1]  # pull only from range sampled by this batch
        frame_idxs_b = torch.clip(frame_idxs+1, min=None, max=self.sequence.shape[0]-1)

        logprob_a = F.log_softmax(self.sequence[frame_idxs, ...], dim=-1)
        logprob_b = F.log_softmax(self.sequence[frame_idxs_b, ...], dim=-1)

        # Pick the highest probability class between the two - this is the class to match
        max_logprob = torch.maximum(logprob_a, logprob_b)
        max_idx = max_logprob.argmax(dim=-1, keepdim=True)

        # negative log-prob of A and B at selected index
        nll_ab = -logprob_b.gather(dim=-1, index=max_idx).squeeze(-1)
        nll_ba = -logprob_a.gather(dim=-1, index=max_idx).squeeze(-1)

        per_tile_loss = (nll_ab + nll_ba) * 0.5
        return per_tile_loss.mean()

    def forward(self, frame_idxs):
        """
        Index into stored sequence weights; use these to assemble a batch of images.

        :param frame_idxs: Batch of frame indices to consider.
        :return: An image reconstructed by combining input blocks.
        """
        block_len = math.prod(block_size)

        # Pluck out selected block weights (one hot per image tile)
        if self.training and self.train_sequence:
            # Use the gumbel-softmax trick to sample our tiles
            block_weights = F.gumbel_softmax(logits=self.sequence[frame_idxs, ...],
                                             tau=self.gs_tau,
                                             hard=False)
        else:
            # Use argmax
            block_weights = F.one_hot(self.sequence[frame_idxs, ...].argmax(dim=-1),
                                      num_classes=self.sequence.size(-1)
                                      ).float()

        # recreate (unfolded) target image from stored sequence
        uf_reconstructed = torch.matmul(block_weights, self.blocks.view(self.n_blocks, block_len)).permute(0, 2, 1)

        # fold back into expected image shape
        reconstructed = self.fold(uf_reconstructed)

        # Apply deblocking filter.
        reconstructed = deblocking_filter(reconstructed, self.lut)

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

    def dump_probs_img(self, out_path):
        with torch.no_grad():
            seq = F.softmax(self.sequence, dim=-1).detach().cpu().numpy()
            seq = seq.reshape(seq.shape[0], -1)
            seq = (seq * 255).astype(np.uint8)
            vis = cv2.applyColorMap(seq, cv2.COLORMAP_TURBO)
            cv2.imwrite(out_path, vis)


class BlockTrainer:
    def __init__(self):

        self.dataset = VideoFrames(device)
        self.dataset.load_data("Touhou - Bad Apple.mp4")

        self.recr = ImageReconstruction(n_sequence=len(self.dataset))

        self.recr.init_blocks_tiledata("kmeans_256_ni_mse.dat")
        # self.recr.init_sequence("stream-kmeans_256_ni.dat")

        self.recr.to(device)
        self.recr.init_sequence_matching(self.dataset)

        # sampe contiguous chunks with random offset
        # we want the tile-frame-to-frame loss to cover only frames also evaluated for sematic loss
        self.sampler = ContigChunkSampler(self.dataset,
                                          batch_size=32,
                                          random_offset=True,
                                          shuffle=True)
        self.data_loader = DataLoader(self.dataset,
                                      batch_sampler=self.sampler)

        self.optim = torch.optim.Adam(
            [
                {"params": self.recr.blocks, "lr": 0.001},
                {"params": self.recr.sequence, "lr": 0.001 * len(self.dataset) / nblocks}  # Categorical distributions are sampled less often than blocks
            ]
        )

        self.perceptual_loss = LPIPS().to(device)

        tstr = time.strftime("%Y-%m-%d_%H-%M-%S")
        self.out_dir = os.path.join("outputs", "ts192_deblocked_" + tstr)
        self.out_blocks_dir = os.path.join(self.out_dir, "blocks")
        self.out_img_dir = os.path.join(self.out_dir, "imgs")
        self.out_probs_dir = os.path.join(self.out_dir, "probs")

        # frames to visualize
        self.viz_frames = [80, 1141, 1659, 1689, 2303]
        self.out_data_dir = os.path.join(self.out_dir, "data")

        # Weighting factor for the tile-change regularization
        self.change_lambda = 0.01

        os.makedirs(self.out_data_dir, exist_ok=False)
        os.makedirs(self.out_blocks_dir, exist_ok=False)
        os.makedirs(self.out_img_dir, exist_ok=False)
        os.makedirs(self.out_probs_dir, exist_ok=False)
        self.viz_dirs = []
        for imgi in self.viz_frames:
            self.viz_dirs.append(os.path.join(self.out_img_dir, "%06d" % imgi))
            os.makedirs(self.viz_dirs[-1], exist_ok=False)

    def dump_reconstructed_frame(self, out_path, frame_n=1141):
        with torch.no_grad():
            idxs = [self.dataset[frame_n][1]]
            recr_frame = self.recr(idxs)
            torchvision.utils.save_image(recr_frame, out_path)

    def train(self):
        self.recr.eval()
        self.recr.dump_grid(os.path.join(self.out_blocks_dir, "000000_init_blocks.png"))
        for vi, vd in zip(self.viz_frames, self.viz_dirs):
            self.dump_reconstructed_frame(os.path.join(vd, "000000_init_img_%04d.png" % vi), vi)
        self.recr.dump_probs_img(os.path.join(self.out_probs_dir, "000000_init_probs.png" ))

        self.recr.train_sequence = True
        self.recr.sequence.requires_grad = True

        # gumbel-softmax temperature annealing - value updates once per epoch (aka once per pass thru video)
        gs_tau_scale = 1.0
        gs_tau_min = 0.1
        gs_tau_anneal_rate = 1e-2

        for epoch in range(1000000):
            self.recr.train()

            epoch_loss = 0.0
            epoch_loss_percep = 0.0
            epoch_loss_change = 0.0

            epoch_n = 0

            self.sampler.set_epoch(epoch)  # update sampler offset and shuffle

            # anneal gumbel-softmax temperature
            self.recr.gs_tau = max(gs_tau_min, gs_tau_scale * np.exp(-gs_tau_anneal_rate * epoch))

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
                    print("Epoch %d Batch %03d: frames (%04d - %04d), Loss %f (percep %f choice %f)" % (epoch, batch_n, min(idx), max(idx), loss.item(), loss_percep.item(), loss_change.item()))

            epoch_loss /= epoch_n
            epoch_loss_percep /= epoch_n
            epoch_loss_change /= epoch_n

            self.recr.eval()

            # write binary video data
            self.recr.dump_blocks(os.path.join(self.out_data_dir, "%05d_%0.06f_p%0.06f_c%0.06f_blocks.dat" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change)))
            self.recr.dump_sequence(os.path.join(self.out_data_dir, "%05d_%0.06f_p%0.06f_c%0.06f_stream.dat" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change)))

            # write visualization for humans
            self.recr.dump_grid(os.path.join(self.out_blocks_dir, "%05d_%0.06f_blocks.png" % (epoch, epoch_loss)))

            for vi, vd in zip(self.viz_frames, self.viz_dirs):
                self.dump_reconstructed_frame(os.path.join(vd, "%04d_%0.06f_img_%04d.png" % (epoch, epoch_loss, vi)), vi)

            self.recr.dump_probs_img(os.path.join(self.out_probs_dir, "%04d_%0.06f_probs.png" % (epoch, epoch_loss)))

            print("Epoch %d: loss %f (percep %f change %f)" % (epoch, epoch_loss, epoch_loss_percep, epoch_loss_change))


def main():
    trainer = BlockTrainer()
    trainer.train()


if __name__ == "__main__":
    main()
