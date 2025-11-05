#!/bin/sh

podman machine start || true > /dev/null 2>&1
podman run -it  -v ./:/data arm-none-eabi-gcc sh -c "cd uECG_v5 && make clean && make"
