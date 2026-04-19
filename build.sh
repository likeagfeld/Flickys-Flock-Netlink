#!/bin/bash
set -e

echo "=== Building Flicky's Flock ==="
export NCPU=$(nproc)
make clean && make -j${NCPU} all

echo "=== Collecting build output for ODE ==="
mkdir -p build

cp -f game.iso build/game.iso
cp -f game.cue build/game.cue
cp -f TRACK1.WAV build/TRACK1.WAV
cp -f TRACK2.WAV build/TRACK2.WAV
cp -f TRACK3.WAV build/TRACK3.WAV
cp -f TRACK4.WAV build/TRACK4.WAV

echo "=== Build complete! ==="
echo "ODE files in build/ folder:"
ls -lh build/
