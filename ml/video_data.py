import torch
from torch.utils.data import Dataset
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
        cap = cv2.VideoCapture(video_path)

        while True:
            r, frame = cap.read()

            if not r:
                break

            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            frame = cv2.resize(frame, (img_size[0], img_size[1]), interpolation=cv2.INTER_AREA)
            frame = frame.astype(np.float32)[None, ...] / 255.0
            frame = torch.from_numpy(frame).to(self.device)
            self.frames.append(frame)

    def __len__(self):
        return len(self.frames)

    def __getitem__(self, idx):
        return self.frames[idx], idx

