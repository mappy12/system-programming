#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int create_text_file(const chat* path, const char* content) {
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