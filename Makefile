obj-m += mmap_kernel.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -g mmap_user.c -o mmap_user

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm mmap_user
