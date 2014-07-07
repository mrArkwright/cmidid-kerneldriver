#!/bin/bash

# Run this script once after restarting the raspberry pi.

# We need some of the dependencies of the snd_virmidid module.
# These are made available by loading and unloading this module.
modprobe snd_virmidi
rmmod snd_virmidi

# FLUID Synth can be used to sample the MIDI output of our module on
# the Raspberry Pi.
killall fluidsynth
fluidsynth -is -g 5 -a alsa /home/pi/TimGM6mb.sf2 &
