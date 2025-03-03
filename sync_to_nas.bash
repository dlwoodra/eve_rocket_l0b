#!/bin/bash

SRC_DIR="/home/evesdp/rl0b/data/level0b"
DEST_DIR="/rocketnasshared/surf/data/"

nice -n 19 ionice -c2 -n7 rsync -av --include="*/" --include="*.fit.gz" \
  --exclude="*" --bwlimit=100000 "$SRC_DIR" "$DEST_DIR"



