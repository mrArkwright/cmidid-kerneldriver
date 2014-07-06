#!/bin/bash

rmmod cmidid
insmod cmidid.ko gpio_mapping=4,22,70

# MODNAME=$(cat cmidid_main.h | grep "#define MODULE_NAME" | awk '{print $3}' | sed -e 's/"//g')
# VERSION=$(dmesg | grep -i "registered character device" | tail -n 1 | awk '{print $7}' | sed -e 's/\.//')
