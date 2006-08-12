#include <sys/types.h>
#include <sys/ata.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>

int main() {
	struct ata_cmd iocmd;
	int fd;

	bzero(&iocmd, sizeof(struct ata_cmd));

	if ((fd = open("/dev/ata", O_RDWR)) < 0)
		err(1, "control device not found");

	iocmd.channel = 0;
	iocmd.cmd = ATAREINIT;
	if (ioctl(fd, IOCATA, &iocmd) < 0)
		warn("ioctl(ATAREINIT)");

	close(fd);
}
