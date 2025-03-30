import numpy as np
from blocksettings import *
from dataclasses import dataclass
from typing import Optional, List
import csv
import os
import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np


blocks_path = "path/to/blocks.dat"
stream_path = "path/to/stream.dat"
tilechanges_path ="path/to/skips.csv"   # Run eval_tilechanges.py first

@dataclass
class SkipElem:
    tile: int
    on_frame: int
    changed_from: int
    changed_to: int
    prev_change: int
    next_change: int
    early_impact: Optional[float]
    late_impact: Optional[float]

    def sort_key(self) -> float:
        return min(self.early_impact, self.late_impact)


def to_int(val: str) -> Optional[int]:
    return int(val) if val.strip() else None


def to_float(val: str) -> Optional[float]:
    return float(val) if val.strip() else None


class SkipTester:
    def __init__(self):
        self.tiles_per_img = img_size[0] * img_size[1] // block_size[0] // block_size[1]

        self.sequence = np.fromfile(stream_path, dtype=np.int32)
        self.sequence = self.sequence.reshape(-1, self.tiles_per_img)

        self.changed_mask = np.zeros(self.sequence.shape, dtype=bool)

        self.target_size = 62000  # bytes

        self.working_dir = "tmp"
        os.makedirs(self.working_dir, exist_ok=True)

        self.skips = []

        self.sz_with_skip = []

    def read_skips_csv(self, csv_path):
        with open(csv_path, newline='') as csvfile:
            reader = csv.DictReader(csvfile)
            for row in reader:
                csv_row = SkipElem(
                    tile=to_int(row['tile']),
                    on_frame=to_int(row['on_frame']),
                    changed_from=to_int(row['changed_from']),
                    changed_to=to_int(row['changed_to']),
                    prev_change=to_int(row['prev_change']),
                    next_change=to_int(row['next_change']),
                    early_impact=to_float(row['early_impact']),
                    late_impact=to_float(row['late_impact'])
                )
                self.skips.append(csv_row)

    def write_stream(self, stream_out_path):
        self.sequence.tofile(stream_out_path)

    def iterative_apply_changes(self):
        self.skips = sorted(self.skips, key=lambda s: s.sort_key())

        n_applied = 0
        for s in self.skips:
            b_change_early = s.early_impact < s.late_impact
            skip_val = s.sort_key()

            # we cannot use this skip if anything in its range has already been touched
            if self.changed_mask[s.prev_change:s.next_change, s.tile].any():
                print(f"Invalid skip (range already touched): f:{self.skips[0].on_frame} t:{self.skips[0].tile} d:{skip_val}")
                continue

            if b_change_early:
                f_start = s.prev_change
                f_end = s.on_frame
            else:
                f_start = s.on_frame
                f_end = s.next_change

            desc = "early" if b_change_early else "late "
            print(f"Apply: {desc} f:{s.on_frame} t:{s.tile} d:{skip_val} n:{f_end-f_start}")

            new_b = s.changed_to if b_change_early else s.changed_from
            self.sequence[f_start:f_end, s.tile] = new_b
            self.changed_mask[f_start:f_end, s.tile] = True
            n_applied += 1

            if skip_val > 0 and n_applied % 10 == 0:
                # Evaluate compression, check if below threshold
                # Note we only do this once skips start causing harm. Negative deltas (improvements) are always applied.
                w_stream_path = os.path.join(self.working_dir, "sk_stream.dat")
                self.write_stream(w_stream_path)

                # Run compressor, look for size in bytes
                cmd = ["vpxtest", w_stream_path, blocks_path, "test.gif"]
                print(cmd)

                pattern = re.compile(r"^\s*Total:\s+(\d+)\s+Bytes", re.MULTILINE)

                try:
                    # Timeout fast - we don't care about the gif, just the size in bytes
                    result = subprocess.run(
                        cmd,
                        capture_output=True,
                        text=True,
                        timeout=100.0
                    )
                    output = result.stdout
                except subprocess.TimeoutExpired as e:
                    output = e.stdout if e.stdout is not None else ""

                match = pattern.search(output)
                if match:
                    sz_bytes = int(match.group(1))
                else:
                    sz_bytes = None

                if sz_bytes is None:
                    print("Error in compression")
                    break

                self.sz_with_skip.append([n_applied, sz_bytes])

                print("Compressed:", sz_bytes)
                if sz_bytes < self.target_size:
                    print("Target reached!")
                    break

        print("Applied", n_applied, "skips")


def main():
    st = SkipTester()
    st.read_skips_csv(csv_path=tilechanges_path)

    st.iterative_apply_changes()

    d = np.array(st.sz_with_skip)
    plt.plot(d[:, 0], d[:, 1])
    plt.xlabel("Applied Skips")
    plt.ylabel("Compressed Size")
    plt.grid()
    plt.title("Compressed Size vs Applied Skips")
    plt.show()


if __name__ == "__main__":
    main()
