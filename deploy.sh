#!/bin/sh
# Copies the build into the game folder (next to this source folder).
set -e
cd "$(dirname "$0")"
cp data/text.tsv ../DBZ3Access/text.tsv
cp out/DBZ3Access/DBZ3Access.dll ../DBZ3Access/DBZ3Access.dll
# The loader wraps the port's graphics plugin. The launcher puts it in place
# (and does so again after any port update), so the build uses it too.
mkdir -p ../DBZ3Access/loader
cp out/rexgpu-access.dll ../DBZ3Access/loader/rexgpu-xenos.dll
cp "out/Play DBZ3 Access.exe" "../Play DBZ3 Access.exe"
"../Play DBZ3 Access.exe" --repair || echo "launcher repair failed; see DBZ3Access/launch.log"
