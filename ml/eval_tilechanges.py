import torch
import torch.nn as nn
import numpy as np
from video_data import VideoFrames
from train_patches_sequence import deblocking_filter, gen_deblocking_lut
from ttools.modules.losses import LPIPS
from blocksettings import *
import tqdm
import os
import csv

"""
Evaluates the semantic impact of tile changes in a video sequence.

Frequent tile changes are not great for compression. To reduce the size of the video, we want to find changes that can
be skipped, ideally without significantly harming the semantic appearance of the video.

We can evaluate the semantic impact of swapping out a given tile by comparing the semantic loss of the modified frame(s)
versus the semantic loss of the original frames (with both comparing against the ground-truth video frame). We generally
expect the modified frame to have higher loss, but this is not necessarily true, especially when deblocking filters are
in play, or when a sequence has been optimized without affordance for tiles switching to different blocks.

For a given change that is skipped (resulting in a sequence of N modified frames), we sum up these loss deltas, and
report the sum S and number of modified frames N. An external process could then search through these changes and modify
the video sequence accordingly, likely with a greedy search of minimum-impact changes until the video is of the desired
compressed size.

Note that this script evaluates only first-order changes; that is, when any given frame is evaluated, only one tile is
changed. The semantic loss is evaluated over whole frames; it would be computationally impractical to consider the 
combination of all possible substitutions (e.g. 6x8 tiles -> 2^48 evaluations of the semantic loss, per frame). Instead,
if time-overlapping substitutions are needed, it may be better to run this script repeatedly on modified sequences.
"""

# Consider a sequence that looks like this:
# 0 1 1 1 2 2 2 3
#
# We can remove the 1->2 transition in the middle two ways:
# 0 1 1 1 1 1 1 3  # "Change-Late" (all 2s replaced by 1s)
# 0 2 2 2 2 3 3 3  # "Change-Early" (all 1s replaced by 2s)
#
# This is true of any transition in the sequence (e.g 0->1 or 2->3), but for this search we consider only single changes
# that don't overlap and don't extend over other changes. This means we can efficiently sample the changes we need to
# evaluate for a given skip by sweeping a latch forward and back over the sequence:
# Orig:         1111222234455
# Change-Late:  ____111123344
# Change-Early: 22223333455__
#
# where any contiguous sequence of not-underscore numbers in change-late or change-early represents a delta.
#
# This leads to a relatively small number of easily-batched evaluations of the loss function, that we can use to build a
# big lookup table for aggregating the impact of any given skip after the fact.

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# inputs
stream_path = "path/to/XXXX_stream.dat"
blocks_path = "path/to/XXXX_blocks.dat"

out_path = stream_path + "_skips.csv"


class SkipEvaluator:
    def __init__(self):
        self.tiles_per_img = img_size[0] * img_size[1] // block_size[0] // block_size[1]

        self.dataset = VideoFrames(device)
        self.dataset.load_data("Touhou - Bad Apple.mp4")

        self.perceptual_loss = LPIPS().to(device)

        # load tiles
        self.tiles = np.fromfile(blocks_path, dtype=np.float32)
        self.tiles = self.tiles.reshape(-1, block_size[0], block_size[1])

        # Load the base sequence as cpu integers from file
        self.base_sequence = np.fromfile(stream_path, dtype=np.int32)
        self.base_sequence = self.base_sequence.reshape(-1, self.tiles_per_img)

        # placeholders for early and late alternate sequences
        self.late_sequence = np.zeros_like(self.base_sequence)
        self.early_sequence = np.zeros_like(self.base_sequence)

        # per-frame per-tile semantic loss delta due to a given tile being swapped out
        self.change_early_ld = np.zeros((len(self.dataset), self.tiles_per_img))
        self.change_late_ld = np.zeros((len(self.dataset), self.tiles_per_img))

        # deblocking filter lut
        self.lut = gen_deblocking_lut()

    def make_change_late_seq(self):
        held_idxs = self.base_sequence[0].copy()
        self.late_sequence[0] = held_idxs
        for i in range(self.base_sequence.shape[0] - 1):
            j = i+1

            change_mask = (self.base_sequence[i] != self.base_sequence[j])
            # print(change_mask)

            if not np.any(change_mask):
                self.late_sequence[j] = held_idxs
                continue

            held_idxs[change_mask] = self.base_sequence[i][change_mask]
            self.late_sequence[j] = held_idxs

    def make_change_early_seq(self):
        base_rev = np.flip(self.base_sequence.copy(), axis=0)
        held_idxs = base_rev[0].copy()
        self.early_sequence[0] = held_idxs
        for i in range(base_rev.shape[0] - 1):
            j = i+1

            change_mask = (base_rev[i] != base_rev[j])
            if not np.any(change_mask):
                self.early_sequence[j] = held_idxs
                continue

            held_idxs[change_mask] = base_rev[i][change_mask]
            self.early_sequence[j] = held_idxs

        self.early_sequence = np.flip(self.early_sequence, axis=0)

    def construct_frame_alts(self, alt_seq, frame_i):
        """
        For a specified frame containing sequences of N tiles, construct a set of N+1 frames, in which:
            - The first frame is an un-modified copy of the sequence at frame_i
            - All others have one tile each drawn from alt_seq
        """
        out_imgs = np.zeros((self.tiles_per_img + 1, 1, img_size[0], img_size[1]))
        base_tileidxs = self.base_sequence[frame_i].copy()

        # make the unmodified frame
        bt = base_tileidxs.reshape(img_size[0]//block_size[0], img_size[1]//block_size[1])
        ba = self.tiles[bt]
        ba = ba.transpose(0, 2, 1, 3)
        out_imgs[0] = ba.reshape(img_size[0], img_size[1])

        for i in range(self.tiles_per_img):
            frame_tileidxs = base_tileidxs.copy()
            frame_tileidxs[i] = alt_seq[frame_i, i]

            # add batch dim / reshape to image
            frame_tileidxs = frame_tileidxs.reshape(img_size[0]//block_size[0], img_size[1]//block_size[1])

            # Assemble frame by indexing into tiles
            assembled_tiles = self.tiles[frame_tileidxs]  # shape: (h, w, block_h, block_w)
            assembled_tiles = assembled_tiles.transpose(0, 2, 1, 3)  # Interleave: (h, block_h, w, block_w)
            out_imgs[i+1] = assembled_tiles.reshape(img_size[0], img_size[1])  # collapse to (h*block_h, w*block_w)

        return out_imgs

    def eval_change_luts(self):
        for frame_idx in tqdm.tqdm(range(len(self.dataset)), desc="Eval change LUTs"):
            gt_frame, _ = self.dataset[frame_idx]
            gt_frame = gt_frame[None, ...]

            # Get unmodified image + set of modified images. Shape: (n_tiles + 1, img_h, img_w)
            imgs_late = self.construct_frame_alts(self.late_sequence, frame_idx)
            imgs_early = self.construct_frame_alts(self.early_sequence, frame_idx)

            imgs_late_t = torch.Tensor(imgs_late).to(device)
            imgs_early_t = torch.Tensor(imgs_early).to(device)

            # apply deblocking filter
            imgs_late_t = deblocking_filter(imgs_late_t, self.lut)
            imgs_early_t = deblocking_filter(imgs_early_t, self.lut)

            # resample
            tgt_us = nn.functional.interpolate(gt_frame, size=(192, 256), mode='nearest')
            rcd_late_us = nn.functional.interpolate(imgs_late_t, size=(192, 256), mode='nearest')
            rcd_early_us = nn.functional.interpolate(imgs_early_t, size=(192, 256), mode='nearest')

            # TODO: Implement our own LPIPS loss with a reduction argument
            #       LPIPS impl forcibly outputs a mean over the batch dim, which means we have to evaluate these one at
            #       a time. It would be better if we could get losses for each batch element (see 'reduction' arg in a
            #       number of built-in torch losses). For now, because this is a loop where it should be a batched eval,
            #       this code is probably slower than it should be.

            l_late = torch.zeros(rcd_late_us.shape[0], dtype=torch.float32, device=device)
            for j in range(rcd_late_us.shape[0]):
                l_late[j] = self.perceptual_loss(rcd_late_us[j][None, ...], tgt_us)

            l_early = torch.zeros(rcd_early_us.shape[0], dtype=torch.float32, device=device)
            for j in range(rcd_late_us.shape[0]):
                l_early[j] = self.perceptual_loss(rcd_early_us[j][None, ...], tgt_us)

            # Store offsets from changed frame
            self.change_late_ld[frame_idx] = (l_late[1:] - l_late[0]).cpu().numpy()
            self.change_early_ld[frame_idx] = (l_early[1:] - l_early[0]).cpu().numpy()

    def collect_change_deltas(self):
        d_base_sequence = np.diff(self.base_sequence, axis=0, prepend=1, append=1)

        with open(out_path, 'w', newline='') as out_file:
            field_names = ["tile", "on_frame", "changed_from", "changed_to",
                           "prev_change", "next_change",
                           "early_impact", "late_impact"]

            out_csv = csv.DictWriter(out_file, fieldnames=field_names)
            out_csv.writeheader()

            for tile_idx in range(self.base_sequence.shape[1]):
                # find frame indices where this tile changed
                change_frame = np.where(d_base_sequence[:, tile_idx] != 0)[0]

                # Collect impact sums
                for j in range(len(change_frame) - 2):
                    # on frame b:
                    #  - this tile changed from whatever it was on since frame a
                    #  - and will stay this way until frame c
                    a = change_frame[j]
                    b = change_frame[j + 1]
                    c = change_frame[j + 2]

                    early_n = b - a
                    early_impact = np.sum(self.change_early_ld[a:b, tile_idx])

                    late_n = c - b
                    late_impact = np.sum(self.change_late_ld[b:c, tile_idx])

                    d = {
                        "tile": tile_idx,
                        "on_frame": b,
                        "changed_from": self.base_sequence[b-1, tile_idx],
                        "changed_to": self.base_sequence[b, tile_idx],
                        "prev_change": a,
                        "next_change": c,
                        "early_impact": early_impact,
                        "late_impact": late_impact
                    }
                    out_csv.writerow(d)

                    # human readable summary for debug
                    # print("Tile", tile_idx, "changes block", self.base_sequence[b-1, tile_idx], "->", self.base_sequence[b, tile_idx], "on frame", b)
                    # print("\tChange", early_n, "frames early on frame", a, ":", early_impact)
                    # print("\tChange", late_n, "frames late on frame", c, ":", late_impact)

    def run(self):

        self.make_change_late_seq()
        self.make_change_early_seq()

        self.eval_change_luts()

        self.collect_change_deltas()


def main():
    se = SkipEvaluator()

    # TODO: perf improvement is possible here if we use a not-Torch backend optimized for inference
    with torch.no_grad():
        se.run()


if __name__ == "__main__":
    main()