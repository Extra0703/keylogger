obj-m := keyboard_simulator.o
prog1 := keyboard_monitor
prog2 := user_application

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	gcc -o $(prog1) $(prog1).c
	gcc -o $(prog2) $(prog2).c
	make -C $(KDIR) M=$(PWD) modules

clean:	
	rm -f $(prog1) $(prog2) *.o
	make -C $(KDIR) M=$(PWD) clean
