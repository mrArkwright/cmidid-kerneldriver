#!/bin/bash

aconnect -x
rmmod cmidid
insmod cmidid.ko
aconnect 20:0 128:0

