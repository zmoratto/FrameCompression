FrameCompression
================

Tools to compress/decompress frames that have metadata in the top 32kb that can't be ruined.

## How to build

You'll need FFMpeg built against libx264. You'll then need to edit the Makefile appropriately.

## How to use

Example to compress data into output.mkv:
./compress data/m%07d.pgm output.mkv 50

The number at the end is the Qmax number. 0 should be lossless ... doesn't seem to work. Higher number means more lossy compression.

Example to decompress the data:
./decompress output.mkv

Example to compare pgms to see how compression was done:
./compare_pgm data/m0000799.pgm output0000799.pgm
emacs comparison.pgm

## How to do lossless using only FFMpeg only
Compress:
ffmpeg -r 30 -i data/m%07d.pgm -c:v libx264 -qp 0 -r 30 lossless.mkv

Decompress:
ffmpeg -r 30 -i lossless.mkv unpack%07d.pgm

You'll notice that the MD5Sum don't match. That is because comment characters, '#', in the PNM Header are missing. Remove comments from the original file with your favorite hex editor and you'll see the MD5's match. Alternatively, you can perform a comparison with ./compare_pgm. 

## Compression Comparison

This is a comparison using 800 frames of small 640x896 PGMs. - 10/31/13

<pre>
RAW: 441M
TAR BZ2: 131M
LOSSLESS: 102M
------------ My Code Below -----
FLAC/LossLess H264 Q=0: 108M
FLAC/Lossy H264 Q=10: 110M
FLAC/Lossy H264 Q=20: 56M
FLAC/Lossy H264 Q=30: 19M
FLAC/Lossy H264 Q=40: 11M
FLAC/Lossy H264 Q=50: 9.3M
FLAC/Lossy H264 Q=60: 9.2M
FLAC/Lossy H264 Q=70: 9.2M
</pre>

I'm not really sure how to pass parameters correctly to Libx264 via FFMpeg. Working on it.
