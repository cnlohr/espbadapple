## Setting up

```
pip3 install torch-tools torch opencv-python torchvision ttools
```

You may need to...
```
pip3 install numpy==1.26.4
```

To setup:
```
cp ../comp2/tiles-64x48x8.dat kmeans_ni_mse.dat
cp ../comp2/badapple-sm8628149.mp4 "Touhou - Bad Apple.mp4"
```

To start operation:
```
python3 train_patches.py | ts | tee runlog.txt
```

This will output files in the `outputs/ts256_.../blocks folder`

Then to extract, run:

```
python3 reconstruct_video_blocks.py outputs/ts256_.../blocks/35_0.972144_blocks.png
```

It will output tilemap in, for instance:

```
35_0.972144_blocks_ts256_2024-12-21_16-57-28/tilemap.dat
35_0.972144_blocks_ts256_2024-12-21_16-57-28/sequence.dat
```

You can copy:
 * `tilemap.dat` to comp2/ as `tiles-${RES}.dat`
 * `sequence.dat` to comp2/ as `stream-${RES}.dat`

Then `make video-64x48x8.gif test.mp4`


