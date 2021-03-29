#!/bin/sh -e

cd "$(dirname $0)"

FEED_PATH="../../../"
if [ ! -d "$FEED_PATH" ]; then
    echo "path to add patches to does not exist: $(dirname $0)/$FEED_PATH"
    exit 1
fi
cp -rv patches/* "$FEED_PATH"
echo
echo "patches added"
echo
