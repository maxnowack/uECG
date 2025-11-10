#!/bin/sh

rm -f tools/replay_ecg
gcc -std=c11 -Wall -Wextra -Itools/stubs -IuECG_v5 -Iurf_lib tools/replay_ecg.c uECG_v5/r_detector.c uECG_v5/ecg_processor.c -lm -o tools/replay_ecg
chmod +x tools/replay_ecg
