#!/bin/sh
# Fills pkg/usr/share/ppsspp/assets from the repository's assets/ directory
# (the RPM spec copies pkg/ as is). Run once after cloning, and again when
# assets/ changes. The two extra images in that directory are tracked.
set -e
cd "$(dirname "$0")/.."
mkdir -p pkg/usr/share/ppsspp/assets
rsync -a --exclude .git assets/ pkg/usr/share/ppsspp/assets/
echo "assets prepared"
