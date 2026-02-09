#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

int create_text_file(const char* path, const char* content) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
	if (fd == -1) return -1;

	size_t len = strlen(content);

	ssize_t written_bites = write(fd, content, len);
	if (written_bites != (ssize_t)len) {
		close(fd);
		errno = (written_bites == -1) ? errno : EIO;
		return -1;
	}

	close(fd);
	return 0;
}

int create_empty_file(const char* path) {
	int fd = open(path, O_CREAT, O_WRONLY | O_TRUNC, S_IRWXU);

	if (fd == -1) {
		return -1;
	}

	close(fd);
	return 0;
}

int create_zero_file(const char* path, size_t size) {
	int fd = open(path, O_CREAT | O_WRONLY, S_IRWXU);
	if (fd == -1) return -1;

	if (ftruncate(fd, (off_t)size) == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}