#!/bin/bash

# Removing old modules from previous runs of this script.
rmmod cmidid

# Start the module. Add the required module parameters here.
# Check with `modinfo cmidid.ko'
insmod cmidid.ko gpio_mapping=17,4,60 jitter_res_time=10000000

