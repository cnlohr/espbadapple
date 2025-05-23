# espbadapple
My shot at bad apple on an '8266.  The source video for bad apple is 512x384


## Prep

```
sudo apt-get install build-essential libavutil-dev libswresample-dev libavcodec-dev libavformat-dev libswscale-dev
```

For msys2 (Windows)

```
pacman -S base-devel mingw-w64-x86_64-ffmpeg 
pacman -S clang llvm clang64/mingw-w64-clang-x86_64-wasm-component-ld mingw-w64-clang-x86_64-wasmer mingw-w64-x86_64-binaryen # For web stuff.
```

### Workflow

#### Non-ML 

```bash
cd comp2
make clean tiles-64x48x8.dat
cd ../streamrecomp
make clean stream_stripped.dat
cd ../song
make clean ../playback/badapple_song_huffman_reverselzss.h track-float-48000.dat
cd ../vpxtest
make clean test.mp4
cd ../playback
make clean playback.mp4
cd interactive
make interactive
cd web
export PATH=${PATH}:/clang64/bin # if on msys2, 
make index.html
```

#### ML

```bash
cd comp2
make clean tiles-64x48x8.dat
cd ../ml
make setup runcomp
```

wait a few minutes

```bash
cd ../ml
make reconstruct
cd ../streamrecomp
make streamrecomp && ./streamrecomp
cd ../vpxtest
make clean test.gif #or test.mp4
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
 * Show original with SKIP_FIRST_AFTER_TRANSITION
 * But could we have the holy grail?  Could we not skip transitions?

Story arc:
 * RLE/Tiles/etc in raster mode.
 * Do it per tile, in time.
 * Don't run multiple unique streams.

After Full VPX
         Run:157208 bits / bytes: 19651
         Run:140136 bits / bytes: 17517
 * But we know more, what if we consider the previous value? - Need another 256-byte table.
         Run:151848 bits / bytes: 18981
         Run:135120 bits / bytes: 16890
 * The missstep of VPX_CODING_ALLOW_BACKTRACK
 * Always do A/B tests, so the absolut doesn't matter but doing A/B to compare them more broadly.


... lots of steps
...

 * Inverting run data goes from 63365 to 63282

Any time you do an experiment with this you make headway.

## Previous Work
| Date | Platform | Storage | Res | Technique | References |
| --- | --- | --- | --- | --- | --- |
| 2011-05-17 | Game Boy Color | 8MB ROM | 160x144 | Unknown | https://emuconsoleexploitnews.blogspot.com/2011/05/bad-apple-demo-ported-to-gameboy-color.html / https://web.archive.org/web/20181013041044/http://www.geocities.jp/submarine600/html/apple.html |
| 2012-05-21 | CASIO fx-CG50 | ??? | 72x54@20FPS | RLE, FastLZ | https://github.com/oxixes/bad-apple-cg50 |
| 2012-10-01 | NES | 512kB | 64x60@15FPS | P/I Frames, Updating Rows of Image (later 30 FPS, but undocumented) | https://wiki.nesdev.com/w/index.php/Bad_Apple / https://www.nesdev.org/wiki/Bad_Apple |
| 2014-01-17 | TI-84 Plys | 2.3MB | 96x64@30FPS | 1bpp, gzipped | https://www.youtube.com/watch?v=6pAeWf3NPNU  |
| 2014-06-15 | 8088 Domination | 19.5MB | 640x200@30FPS | Row-at-a-time frame-deltas | https://www.youtube.com/watch?v=MWdG413nNkI - https://trixter.oldskool.org/2014/06/19/8088-domination-post-mortem-part-1/ |
| 2014-06-29 | Commodore 64 | 170kB | 312x184@12FPS | Full Video, glyphs (16x16) | https://www.youtube.com/watch?v=OsDy-4L6-tQ |
| 2015-05-09 | Vectrex | ??? | ??? | ??? | https://www.youtube.com/watch?v=_aFXvoTnsBU - https://web.archive.org/web/20210108203352/http://retrogamingmagazine.com/2015/07/16/bad-apple-ported-to-the-vectrex-something-that-should-technically-not-be-possible/ - http://spritesmods.com/?art=veccart&page=5 |
| 2016-02-13 | Bad Apple!! zx spectrum [512] version by Techno Lab | 640kB | 256x192@6FPS | Unknown | https://www.youtube.com/watch?v=cd5iEeIe7L0 |
| 2018-01-21 | Arduino Mega "Bad Duino" | 220,924 bytes | 128x176@60FPS | Lossy line-at-a-time deltas | https://rv6502.ca/2018/01/22/bad-duino-bad-apple-on-arduino/ https://www.youtube.com/watch?v=IWJmK5J8shY |
| 2018-09-30 | Atari 400 | 41MB | 384x232@50FPS | ANTIC hardware compression | https://www.wudsn.com/index.php/productions-atari800/demos/badapplehd |
| 2022-11-01 | CHIP-8 | 63kB | 63x32@15FPS | Unkown, appears to be heirarchical | https://koorogi.itch.io/bad-apple |
| 2022-11-03 | CHIP-8 | 61kB | 48x32@15FPS | Frame Diff, reduced diff, post processing, bounding box, huffman trees | https://github.com/Timendus/chip-8-bad-apple |
| 2022-01-01 | Thumbboy | 289kB | 52x39@30FPS +DPCM Audio | ??? | https://www.youtube.com/watch?v=vbBQ11BZWoU |
| 2024-01-01 | NES (TAS ACE exploit via Mario Bros) | ??? | 15/30FPS (160x120) | 20x15 tiles glyph'd using Mario glyphs | https://www.youtube.com/watch?v=Wa0u1CjGtEQ https://tasvideos.org/8991S |
| 2025-03-05 | SSH Keys | N/A | 17x9 | bad apple but it's ssh keys | https://www.5snb.club/posts/2025/bad-apple-but-its-ssh-keys/ |

### TODO
 * Double check hero frames.
 * Write algo to find missing glyph.
 * Find way of recovering from poorly fitted data, and generating new cells.
 * Visual hyperspectral output.

 * For hufftree, make it so that it STRICTLY goes if 0, earlier, if 1, later.

### Various techniques outlined on forum post

https://tiplanet.org/forum/viewtopic.php?t=24951

 * TI-83+ Silver - fb39ca4 - 1.5MB, 96x64, Full Video - Unknown Technique
 * Graph 75+E - ac100v - 1.27MB - 85x63, Full Video - Unknown Technique
 * Graph 90+E - Loieducode / Gooseling - 64x56, Greyscale
 * Numworks - Probably very large - M4x1m3 - 320x240
 * fx-92 College 2D+ - - 96x31 - 
 
