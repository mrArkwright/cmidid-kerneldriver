# Module (.ko) source file:
obj-m += cmidid.o
# Other source files:
cmidid-objs := cmidid_midi.o cmidid_main.o cmidid_gpio.o

SRC := $(shell pwd)
KSRC:=/lib/modules/$(shell uname -r)/build

SRC_FILES := $(wildcard *.c)

all: indent ioctl_test
	$(MAKE) -C $(KSRC)  M=$(SRC) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

indent:
	indent -linux $(SRC_FILES)

ioctl_test: ioctl_test.c
	gcc $^ -o $@
