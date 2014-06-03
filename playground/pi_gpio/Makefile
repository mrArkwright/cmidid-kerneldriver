obj-m += pi_gpio.o

all: indent
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

indent:
	indent -linux pi_gpio.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
