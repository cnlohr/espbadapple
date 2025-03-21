import torch
from torch.utils.data import Dataset
import av
import cv2
import numpy as np
from blocksettings import *


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

