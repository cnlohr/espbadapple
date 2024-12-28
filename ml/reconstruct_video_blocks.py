import cv2
import numpy as np
import matplotlib.pyplot as plt
import os
import sys
from blocksettings import *

class VideoBuilder:
    def __init__(self):
        self.blocks = []

        self.usage_stats = None
        self.block_sequence = []

    def load_from_grid(self, grid_path):
        grid = cv2.imread(grid_path, cv2.IMREAD_GRAYSCALE)
        height, width = grid.shape
        for i in range(int(height/(block_size[0]+2))):
            for j in range(int(width/(block_size[1]+2))):

                bx = 2 + (block_size[0]+2)*i
                by = 2 + (block_size[1]+2)*j

                b = grid[bx:bx+block_size[0], by:by+block_size[1]].astype(float) / 255.0
                self.blocks.append(b)
        self.usage_stats = np.zeros(len(self.blocks), dtype=int)

    def match_block(self, frame_block):
        min_err = 1e9
        min_idx = None

        # print("frame_block:", frame_block.shape)

        for i, b in enumerate(self.blocks):
            err = np.mean(np.square(b - frame_block))
            if err < min_err:
                min_err = err
                min_idx = i

        return min_idx

    def match_frame(self, frame):
        block_idxs = []

        dw = frame.shape[1] // block_size[0]
        dh = frame.shape[0] // block_size[1]
        for j in range(dh):
            for i in range(dw):
                frame_block = frame[j*block_size[0]:(j+1)*block_size[0], i*block_size[1]:(i+1)*block_size[1]]
                match_idx = self.match_block(frame_block)
                self.usage_stats[match_idx] += 1

                block_idxs.append(match_idx)

        return block_idxs

    def build_frame(self, block_idxs):
        frame = np.zeros((img_size[0], img_size[1]), dtype=np.float32)
        dw = frame.shape[1] // block_size[0]
        dh = frame.shape[0] // block_size[1]
        for j in range(dh):
            for i in range(dw):
                n = block_idxs[j*dw + i]
                b = self.blocks[n]
                frame[j*block_size[0]:(j+1)*block_size[0], i*block_size[1]:(i+1)*block_size[1]] = b

        return frame

    def process_video(self, video_path, out_dir, write_video=False):
        os.makedirs(out_dir, exist_ok=True)

        cap = cv2.VideoCapture(video_path)
        i = 0
        while True:
            r, frame = cap.read()

            if not r:
                break

            print(i)

            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            frame = cv2.resize(frame, (img_size[1], img_size[0]), interpolation=cv2.INTER_AREA)
            frame = frame.astype(np.float32) / 255.0

            block_ids = self.match_frame(frame)

            self.block_sequence.append(block_ids)

            if write_video:
                recr_frame = self.build_frame(block_ids)

                recr_frame = np.clip(recr_frame * 255.0, 0, 255).astype(np.uint8)
                cv2.imwrite(os.path.join(out_dir, "%06d.png" % i), recr_frame)

            i += 1


if __name__ == "__main__":
    grid_path = sys.argv[1]

    out_dir = os.path.basename(grid_path)[:-4] + "_" + os.path.normpath(grid_path).split(os.sep)[-3]
    os.makedirs(out_dir, exist_ok=True)

    builder = VideoBuilder()
    builder.load_from_grid(grid_path)

    # save tiles file
    np.array(builder.blocks, dtype=np.float32).tofile(os.path.join(out_dir, "tilemap.dat"))

    builder.process_video("Touhou - Bad Apple.mp4", out_dir, write_video=True)

    # Sequence of block IDs per frame as 32 bit ints
    block_info = np.array(builder.block_sequence, dtype=np.uint32)
    block_info.tofile(os.path.join(out_dir, "sequence.dat"))

    # print stats
    stats = builder.usage_stats

    stats = list(enumerate(stats))
    stats = sorted(stats, key=lambda x: x[1])
    for i, x in stats:
        print("block %d: %d" % (i, x))
