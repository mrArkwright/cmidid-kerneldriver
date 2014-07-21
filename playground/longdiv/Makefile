obj-m := longdiv.o

ccflags-y := -DDEBUG

KSRC := /lib/modules/`uname -r`/build

all: indent
	$(MAKE) -C $(KSRC) M=$$PWD modules

clean:
	$(MAKE) -C $(KSRC) M=$$PWD clean

indent:
	indent -linux $(wildcard *.c)
