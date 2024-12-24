# espbadapple
My shot at bad apple on an '8266.  The source video for bad apple is 512x384


## Prep

```
sudo apt-get install build-essential libavutil-dev libswresample-dev libavcodec-dev libavformat-dev libswscale-dev
```

### Future TODO
 * Perceptual/Semantic Loss Function
 * De-Blocking Filter
 * Motion Vectors
 * Reference previous tiles.
 * Add color inversion option for glyphs
 * https://engineering.fb.com/2016/08/31/core-infra/smaller-and-faster-data-compression-with-zstandard/
 * https://github.com/webmproject/libvpx/blob/main/vpx_dsp/bitreader.c
 * https://github.com/webmproject/libvpx/blob/main/vpx_dsp/bitwriter.c

## History

My thought to port bad apple started back in early 2017.  But I quickly ran out of steam.

### RLE

My first attempt was with RLE on the raster image, and I tried a number of other tricks, like variable width integers, etc.  But nothing could get it compress all that much.  With the 512x384 res, it took about 1kB per frame to store.  Total size was about 11,602,173 bytes.

### What is this C64 demo doing?

It wasn't until Brendan Becker showed me https://www.youtube.com/watch?v=OsDy-4L6-tQ -- One of the clear huge boons is that the charset.  They claim 70 bytes/frame, at 12fps.

It's clear that they're doing a 40x25 grid of 8x8's.  That would be 1,000 cells? At any rate

### The lull

I somehow lost interest since there wasn't any really compelling and cool combination that could fit on the ESP8285.  So I stopped playing around until 2023, when the CH32V003 came out.  I continued to refine my algorithms, and playing.

By really pushing things to the limits, one could theoretically fit bad apple in XXX kb **TODO IMAGE**

Then, I spent a while talking to my brother and his wife, both PhD's in math, and they introduced me to the K-means algorithm.  But by this time, I had been fatigued by bad apple.

But, then, in early 2024, things really got into high gear again, because WCH, the creators of the CH32V003, announced other chips in the series with FLASH ranging from 32kB to 62kB, so it was time for the rubber to hit the road again.

I implemented a k-means approach, and wowee! The tiles that came out of k-means was AMAZING!!!

### Other notes
 * I got 15% savings when I broke the "run length" and "glyph ID" fields apart.
 * When having split tables, I tried exponential-golomb coding, and it didn't help.  Savings was not worth it.
 * Tride VPX with tiles, it was awful.
 * Tried VPX with RLE.  It beat huffman. (USE_VPX_LEN)
   * Huffman tree RLE (451878 bits)
   * VPX originally per tile (441250 bits)
   * VPX when unified (440610 bits)!!


## Previous Work
 * Commodore 64 - 170kB, 312x184@12FPS, Full Video, glyphs (16x16) -  https://www.youtube.com/watch?v=OsDy-4L6-tQ
 * CHIP-8 - 61kB of video data, 32x48@15FPS, Full Video, implementing techniques like frame diff, reduced diff, post processing, bounding box, huffman trees. - https://github.com/Timendus/chip-8-bad-apple
 * NES - 512kB, 64x60@15FPS, P/I Frames, Updating Rows of Image (later 30 FPS, but undocumented)  - https://wiki.nesdev.com/w/index.php/Bad_Apple
 * NES (TAS ACE exploid via Mario Bros) - 15/30FPS (160x120), 20x15 tiles - glyph'd using Mario glyphs. https://www.youtube.com/watch?v=Wa0u1CjGtEQ
 * FX-CG50 - 72x54@20FPS, RLE, FastLZ - https://github.com/oxixes/bad-apple-cg50
 * Thumbboy - https://www.youtube.com/watch?v=vbBQ11BZWoU - 52x39@30FPS, 45 bytes per frame + DPCM Audio

### Various techniques outlined on forum post

https://tiplanet.org/forum/viewtopic.php?t=24951

 * TI-83+ Silver - fb39ca4 - 1.5MB, 96x64, Full Video - Unknown Technique
 * Graph 75+E - ac100v - 1.27MB - 85x63, Full Video - Unknown Technique
 * Graph 90+E - Loieducode / Gooseling - 64x56, Greyscale
 * Numworks - Probably very large - M4x1m3 - 320x240
 * fx-92 College 2D+ - - 96x31 - 
 
