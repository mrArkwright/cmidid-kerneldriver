#!/bin/bash

# Run this script once after restarting the raspberry pi.

modprobe snd_virmidi
rmmod snd_virmidi

fluidsynth -is -a alsa /home/pi/TimGM6mb.sf2 &
