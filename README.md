# espbadapple
My shot at bad apple on an '8266.  The source video for bad apple is 512x384


## Prep

```
sudo apt-get install build-essential libavutil-dev libswresample-dev libavcodec-dev libavformat-dev
```



## Compression mechanisms.

### RLE

My first attempt was with RLE on the raster image, and I tried a number of other tricks, like variable width integers, etc.  But nothing could get it compress all that much.  With the 512x384 res, it took about 1kB per frame to store.  Total size was about 11,602,173 bytes.

### What is this C64 demo doing?

It wasn't until Brendan Becker showed me https://www.youtube.com/watch?v=OsDy-4L6-tQ -- One of the clear huge boons is that the charset.  They claim 70 bytes/frame, at 12fps.

It's clear that they're doing a 40x25 grid of 8x8's.  That would be 1,000 cells?


### Bad apple on the NES.

They outline the mechaism here: https://wiki.nesdev.com/w/index.php/Bad_Apple

The basic idea is 
