obj-m += new_midi.o

all: ioctl_test
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

ioctl_test: ioctl_test.c
	gcc $^ -o $@

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm ioctl_test