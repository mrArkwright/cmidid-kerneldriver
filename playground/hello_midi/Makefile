obj-m += hello_midi.o
SRC := $(shell pwd)
KSRC:=/lib/modules/$(shell uname -r)/build
#KSRC:=/home/pi/linux-rpi-3.10.y

all: indent
	$(MAKE) -C $(KSRC)  M=$(SRC) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

indent:
	indent -linux hello_midi.c
