#!/bin/sh
# Builds "Budokai 3 Original Music.exe" with the 64-bit w64devkit into ../out/music/.
set -e
cd "$(dirname "$0")"
W64DEVKIT="${W64DEVKIT:-$HOME/tools/w64x64/w64devkit}"
export PATH="$W64DEVKIT/bin:$PATH"
mkdir -p ../out/music
windres og_music.rc -O coff -o ../out/music/og_music_res.o
gcc -O2 -Wall -s -municode -o "../out/music/Budokai 3 Original Music.exe" og_music.c ../out/music/og_music_res.o \
  -static-libgcc -lcomdlg32 -lshell32 -lole32 -luuid -ladvapi32
echo built "../out/music/Budokai 3 Original Music.exe"
