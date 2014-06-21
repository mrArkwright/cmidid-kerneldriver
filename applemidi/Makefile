obj-m += applertp.o
applertp-objs := applemidi.o rtp.o rtpmidi.o midi.o clock.o _uldivmod.o alsa.o
#obj-m += applemidi.o

SRC := $(shell pwd)
KSRC:=/lib/modules/$(shell uname -r)/build

SRC_FILES := $(wildcard *.c)

#indent

all: 
	$(MAKE) -C $(KSRC)  M=$(SRC) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

indent:
	indent -linux $(SRC_FILES)
