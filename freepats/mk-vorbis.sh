#!/usr/bin/env bash
# This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
set -Eeuo pipefail # -x
SCRIPTNAME=`basename $0` && function die  { [ -n "$*" ] && echo "$SCRIPTNAME: **ERROR**: ${*:-aborting}" >&2; exit 127 ; }    

# Extract the samples of a GUS .pat files as WAV files.
# Compile gus2wav.cc
# Encode WAV files to OGG Vorbis.
# Create tarball of the OGG Vorbis files.

VERSION=`git describe --match='v[0-9]*.[0-9]*'`
TARBALL=freepats-vorbis-${VERSION#v}.tar

rm -rf gus2wav ./freepats ./$TARBALL*

test -r gus2wav.cc ||
  die "gus2wav.cc not found; run script from the directory containing gus2wav.cc"

CCACHE="${CCACHE_DIR+ccache}"
test -x gus2wav ||
  ${CCACHE} g++ -O3 -Wall -Wextra -std=c++17 gus2wav.cc -o gus2wav

test -d ./freepats ||
  tar -xvjf freepats-20060219.tar.bz2

test -d freepats/Drum/ || (
  mkdir -p freepats/Drum
  cd freepats/Drum/
  for f in ../Drum_000/*.pat ; do
    ../../gus2wav "$f" || break
  done
)

test -d freepats/Tone/ || (
  mkdir -p freepats/Tone
  cd freepats/Tone/
  for f in ../Tone_000/*.pat ; do
    ../../gus2wav "$f" || break
  done
)

test -n "$(find freepats/Tone/ -maxdepth 1 -name '*.ogg')" || (
  rm -f freepats/*/*.ogg
  for w in freepats/*/*.wav ; do
    oggenc -q4 "$w" -o "${w%.wav}.ogg" || break
  done
)

echo 'du -hsc *.wav'
du -hsc freepats/*/*.wav | tail -1
echo 'du -hsc *.ogg'
du -hsc freepats/*/*.ogg | tail -1

FILES=( README.md ) # gus2wav.cc
cp "${FILES[@]}" freepats/
cd freepats/
tar --transform 's,^,freepats-vorbis/,' "${FILES[@]}" -cf ../$TARBALL */*.ogg
cd -

ls -lh $TARBALL*
zstd -22 --ultra --rm -f $TARBALL
ls -lh $TARBALL*
