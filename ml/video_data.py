import torch
from torch.utils.data import Dataset, Sampler
import av
import cv2
import numpy as np
from blocksettings import *
import random


class VideoFrames(Dataset):
    """
    Loads a video, downsamples it, and caches all the frames on the specified device.
    """
    def __init__(self, device):
        self.device = device
        self.frames = []

    def load_data(self, video_path):
        container = av.open(video_path)

        for frame in container.decode(video=0):
            frame_np = frame.to_ndarray(format='bgr24')
            frame_np = cv2.cvtColor(frame_np, cv2.COLOR_BGR2GRAY)
            frame_np = cv2.resize(frame_np, (img_size[1], img_size[0]), interpolation=cv2.INTER_AREA)
            frame_np = frame_np.astype(np.float32)[None, ...] / 255.0
            frame_np = torch.from_numpy(frame_np).to(self.device)
            self.frames.append(frame_np)

    def __len__(self):
        return len(self.frames)

    def __getitem__(self, idx):
        return self.frames[idx], idx


class ContigChunkSampler(Sampler):
    """
    Yields batches made of contiguous chunks of a dataset, in a random order, with a random offset changing on epochs.
    """
    def __init__(self, data_source, batch_size, random_offset, shuffle):
        self.data_source = data_source
        self.batch_size = batch_size
        self.n_samples = len(data_source)
        self.random_offset = random_offset
        self.shuffle = shuffle
        self.epoch = 0
        self._chunks = None  # Will store the list of batches for the current epoch.

        # Initialize with a default epoch (0)
        self.set_epoch(self.epoch)

    def set_epoch(self, epoch):
        """
        Set the epoch and compute the contiguous batches (chunks) based on
        a random offset that is seeded with the epoch number.
        """
        self.epoch = epoch
        rng = random.Random(self.epoch)

        offset = 0
        if self.random_offset:
            offset = rng.randint(0, self.batch_size - 1) if self.n_samples > self.batch_size else 0

        chunks = []
        # If offset is nonzero, the first chunk will be partial.
        if offset > 0:
            chunks.append(list(range(0, offset)))

        for i in range(offset, self.n_samples, self.batch_size):
            chunks.append(list(range(i, min(i + self.batch_size, self.n_samples))))

        if self.shuffle:
            rng.shuffle(chunks)

        self._chunks = chunks

    def __iter__(self):
        if self._chunks is None:
            self.set_epoch(self.epoch)
        for chunk in self._chunks:
            yield chunk

    def __len__(self):
        if self._chunks is None:
            self.set_epoch(self.epoch)

        return len(self._chunks)
