obj-m += echodev-drv.o

all:
	make -C ../linux-6.1.58 M=$(PWD) modules

clean:
	make -C ../linux-6.1.58 M=$(PWD) clean
