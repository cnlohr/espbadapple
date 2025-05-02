import cv2
import numpy as np
from blocksettings import *
import matplotlib.pyplot as plt
import os
import tqdm
import torch
import torch.nn as nn
from video_data import VideoFrames
from train_patches_sequence import ImageReconstruction
from ttools.modules.losses import LPIPS

"""
Produces frames from reconstructed video and evaluates semantic loss. 
"""

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
# device = 'cpu'


tiles_per_img = img_size[0] * img_size[1] // block_size[0] // block_size[1]

stream_path = "path/to/XXXX_stream.dat"
blocks_path = "path/to/XXXX_blocks.dat"
out_path = "path/to/frames_out_dir"
os.makedirs(out_path, exist_ok=True)

# Load video data
dataset = VideoFrames(device)
print("Load video...")
dataset.load_data("Touhou - Bad Apple.mp4")

# Load reconstruction
print("Load reconstruction...")
recr = ImageReconstruction(n_sequence=len(dataset), quantize=True)
recr.init_blocks_tiledata(blocks_path)
recr.init_sequence(stream_path)
recr.to(device)
recr.eval()

# Setup semantic loss
perceptual_loss = LPIPS().to(device)

frame_percep = []

with torch.no_grad():
    for target_img, idx in tqdm.tqdm(dataset):
        img_recr = recr(torch.tensor([idx], device=device))

        # upsample
        tgt_us = nn.functional.interpolate(target_img[None, ...], size=(192, 256), mode='nearest')
        rcd_us = nn.functional.interpolate(img_recr, size=(192, 256), mode='nearest')

        # Log frame loss
        frame_percep.append(perceptual_loss(rcd_us, tgt_us).item())

        # save frame
        img_np = img_recr.cpu().numpy().squeeze()
        img_np = (img_np * 255.0).astype(np.uint8)

        write_path = os.path.join(out_path, "%06d.png" % idx)

        cv2.imwrite(filename=os.path.join(out_path, "%06d.png" % idx),
                    img=img_np)

# Plot reconstruction loss per frame
plt.plot(frame_percep)
plt.xlabel("Frame Number")
plt.ylabel("Perceptual Loss")
plt.grid()
plt.title("Perceptual Loss per Frame")
plt.show()
