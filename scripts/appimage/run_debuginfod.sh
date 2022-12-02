#!/bin/bash

docker run -p 12345:12345 -it ghcr.io/kdab/kdesrc-build \
    debuginfod -p 12345 -F /usr -v
