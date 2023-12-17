#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "echodev-cmd.h"

int main(int argc, char **argv)
{
	int fd, status;
	uint32_t value;

	if(argc != 3 && argc != 4) {
		printf("USage: %s <devfile> <cmd> [<arg>]\n", argv[0]);
		return 0;
	}

	fd = open(argv[1], O_RDWR);
	if(fd < 0) {
		perror("open");
		return fd;
	}

	if(strcmp(argv[2], "GET_ID") == 0) {
		status = ioctl(fd, GET_ID, &value);
		printf("ioctl returned %d, ID Register: 0x%x\n", status, value);
	} else if(strcmp(argv[2], "GET_INV") == 0) {
		status = ioctl(fd, GET_INV, &value);
		printf("ioctl returned %d, Inverse Pattern Register: 0x%x\n", status, value);
	} else if(strcmp(argv[2], "GET_RAND") == 0) {
		status = ioctl(fd, GET_RAND, &value);
		printf("ioctl returned %d, Random Value Register: 0x%x\n", status, value);
	} else if (strcmp(argv[2], "SET_INV") == 0) {
		value = strtol(argv[3], 0, 0);
		status = ioctl(fd, SET_INV, &value);
		printf("ioctl returned %d\n", status);
	} else {
		printf("%s is not a valid cmd\n", argv[2]);
	}


	close(fd);
	return 0;
}
