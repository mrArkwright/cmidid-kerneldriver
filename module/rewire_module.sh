#!/bin/bash

aconnect -x

./start_module.sh

CMIDID_PORT=$(aconnect -i | grep -i "cmidid" | awk '{print $2}' | sed -e 's/://')
echo "Using ALSA port ${CMIDID_PORT} for our cmidid kernel module."

FSYNTH_PORT=$(aconnect -o | grep -i "fluid synth" | awk '{print $2}' | sed -e 's/://')
echo "Using ALSA port ${FSYNTH_PORT} for FLUID Synth."

aconnect ${CMIDID_PORT}:0 ${FSYNTH_PORT}:0
