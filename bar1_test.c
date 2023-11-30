#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
	int fd;
	void *bar1;
	uint32_t width, offset;
	uint64_t value;

	if((argc != 4) && (argc != 5)) {
		printf("Usage: %s <devfile> <access_width> <offset> [<value>]\n", argv[0]);
		return 0;
	}

	fd = open(argv[1], O_RDWR);
	if(fd < 0) {
		perror("open");
		return -1;
	}

	bar1 = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	width = strtol(argv[2], 0, 0);
	offset = strtol(argv[3], 0, 0);

	if(argc == 4) {
		/* REad */
		if(width == 8) {
			uint8_t *ptr = (uint8_t *) (bar1 + offset);
			printf("0x%x\n", *ptr);
		}
		if(width == 16) {
			uint16_t *ptr = (uint16_t *) (bar1 + offset);
			printf("0x%x\n", *ptr);
		}
		if(width == 32) {
			uint32_t *ptr = (uint32_t *) (bar1 + offset);
			printf("0x%x\n", *ptr);
		}
		if(width == 64) {
			uint64_t *ptr = (uint64_t *) (bar1 + offset);
			printf("0x%llx\n", *ptr);
		}
	} else if(argc == 5) {
		value = (uint64_t) strtoll(argv[4], 0, 0);
		/* Write */
		if(width == 8) {
			uint8_t *ptr = (uint8_t *) (bar1 + offset);
			*ptr = value;
		}
		if(width == 16) {
			uint16_t *ptr = (uint16_t *) (bar1 + offset);
			*ptr = value;
		}
		if(width == 32) {
			uint32_t *ptr = (uint32_t *) (bar1 + offset);
			*ptr = value;
		}
		if(width == 64) {
			uint64_t *ptr = (uint64_t *) (bar1 + offset);
			*ptr = value;
		}
	}

	munmap(bar1, 4096);
	close(fd);
	return 0;
}
