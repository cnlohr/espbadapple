# Very compressed song-maker

## Using BadApple-mod.mid

`BadApple-mod.mid` originally has unknown origin for source but is a midi take on "Bad Apple!!" by Zun and was on Touhou. It was uploaded On 2015-03-01 by livingston26 and uploaded [here](https://musescore.com/user/1467236/scores/678091), but credit was given to cherry_berry17.  I was unable to find them.  Then it was transcribed to MIDI.

I heavily modified it to fit the music video more specifically and also match the restricted hardware situation I am operating in.

If anyone knows the original author this passed through, I'd really appreciate knowing. But, it's so heavily modified, I don't think it would be recognizable.

In general this is a cover of the "Bad Apple" version by Alstroemeria Records featuring nomico.  And depending on jurisdiction, they may maintain some form of copyright.  I disclaim all copyright from the song and midi file.  Feel free to treat my transformation of it under public domain.

## Results

To do sanity checks, I decided to compare the compression a few steps along the way.  (All sizes in octets (bytes))

| Compression | .xml | .json (jq formatting) | .json.min | .mid | .dat | 
| -- | -- | -- |
| uncompressed | 160952 | 111498 | 63483 | 12755 | 2824 |
| [heatshrink](https://github.com/atomicobject/heatshrink) | 23926 | 17599 | 11255 | 2345 | 881
| gzip -9      | 9145 | 8662 | 7986 | 1042 | 568 |
| zstd -9      | 7245 | 6828 | 6429 | 1042 | 594 |
| bzip2 -9     | 6818 | 6692 | 6518 | 1105 | 720 |

Curiously for small payloads, it looks like gzip outperforms zstd, in spite of zstd having 40 years to improve over it.

There's an issue, all of the good ones in this list these are state of the art algorithms requiring a pretty serious OS to decode.  What if we only wan to run on a microcontroller?

Then when ingesting the data into my algorithms:

| Compression | Size |
| Huffman (1 table) | 1465 |
| Huffman (2 table) | 1336 |
| Huffman (3 table) | 1245 |
| VPX (no LZSS) | 1269 |

Note the uptick in size because to use VPX, you have to have a probability table, and huffman tables can be used in lower compression arenas to more effectivity. 

I tried doing huffman + LZSS but it got unweildy.

Then I decided to do a 1/2 hour experiment, and hook up VPX (with probability trees) with LZSS, (heatshrink-style).  

Boom.

600 bytes on the first try.

I then also used entropy coding to encode the run lengths and indexes where I assumed the numbers were smaller, so for small jumps, it would use less bits, and it went down to a final amount of **554 bytes**!

So, not only is our decoder only about 50 lines of code, orders of magnitude simpler than any of the big boy compression algorithms... It eeks out just a little more compression than they can muster!

## Mechanism

To produce the audio file for use with ffmpeg, `track-float-48000.dat`, as well as producing `badapple_song.h` in the `playback/` folder, you can use the following command:

```
make track-float-48000.dat ../playback/badapple_song.h
```

### Interprets `BadApple-mod.mid` and outputs `fmraw.dat` using `midiexport`

Midiexport reads in the .mid file and converts the notes into a series of note hits that has both the note to hit, the length of time the note should play (in 16th notes) and how many 16th notes between the playing of this note and the start of the next note.

This intermediate format is a series of `uint16_t`'s, where the note information is stored as:

```c
int note = (s>>8)&0xff;
int len = (s>>3)&0x1f;
int run = (s)&0x07;
```

Assuming a little endian system.

### Ingests `fmraw.dat` and produces `../playback/badapple_song.h`

This employs a 4-part compression algorithm.

1. Uses a [heathrink](https://github.com/atomicobject/heatshrink)-like compression algorithm, based on [LZSS](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Storer%E2%80%93Szymanski) with the twist that it uses the VPX entropy coding to reduce the cost of smaller jumps and lengths.  So on avearge it can outperform heatshrink or LZSS.
2. Then creates two probability trees.  One for notes, the other for (note length | duration until next note).
3. NOTE: When creating the probabilities or encoding the data, it only considers the probability of the remaining notes after LZSS.  This is crucial for good compression.
4. It creates two VPX probability trees for these two values.
5. It creates another table so that it can tightly pack the (length | duration) tree.
6. It produces a VPX bitstream output.

The .dat file can be used with ffmpeg as follows:

```
ffmpeg -y -f f32le -ar 48000 -ac 1 -i ../song/track-float-48000.dat <video data> <output.mp4>
```

The .h file is used with the playback system by being compiled in.




