#!/bin/bash
gcc -I/home/gabriel/code/code/c/transcoder logger.c main.c transcode_utils.c filters/filter.c filters/edges.c \
filters/ffmpeg_filters.c filters/scaler.c filters/audio_encoder.c filters/video_encoder.c -o compiled.out \
`pkg-config --libs libavformat libavfilter libavutil libavcodec libswscale libavdevice libavutil` \
-pthread  -lltdl -lcrypt -lm -lltdl 