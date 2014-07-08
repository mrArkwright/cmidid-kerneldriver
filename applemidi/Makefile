obj-m += applertp.o

#for raspi
applertp-objs := applemidi.o rtp.o rtpmidi.o midi.o clock.o alsa.o message.o _uldivmod.o

#for systes with __aeabi_uldivmod use this line and comment the one above
#applertp-objs := applemidi.o rtp.o rtpmidi.o midi.o clock.o alsa.o message.o

#if you need a lot of debug ouput, uncomment the following line
#ccflags-y := -DDEBUG

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
