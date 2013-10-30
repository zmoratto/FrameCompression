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

This is a comparison using 800 frames of small 640x896 PGMs. - 10/30/13

RAW: 441M
TAR BZ2: 131M
LOSSLESS: 102M
THIS Q=0: 136M
THIS Q=10: 94M
THIS Q=20: 43M
THIS Q=30: 22M
THIS Q=40: 18M
THIS Q=50: 17M
THIS Q=60: 17M
THIS Q=70: 17M

I'm not really sure how to pass parameters correctly to Libx264 via FFMpeg. Working on it.
