#!/usr/bin/python

import array, fcntl, struct, termios, os

fd = open('/dev/cmidid', 'w')
fcntl.ioctl(fd, 0);
fd.close
