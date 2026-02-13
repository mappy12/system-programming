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
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

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

int create_random_file(const char* path, size_t size) {
	int fd_to = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

	if (fd_to == -1) {
		return -1;
	}

	int fd_rand = open("/dev/urandom", O_RDONLY);
	if (fd_rand == -1) {
		close(fd_to);
		return -1;
	}

	unsigned char buffer[4096];
	size_t remaining_size = size;

	while (remaining_size > 0) {
		size_t to_read = (remaining_size < sizeof(buffer))
		? remaining_size
		: sizeof(buffer);

		ssize_t actually_read = read(fd_rand, buffer, to_read);

		if (actually_read <= 0) {
			close(fd_rand);
			close(fd_to);
			return -1;
		}

		if (write(fd_to, buffer, actually_read) != actually_read) {
			close(fd_rand);
			close(fd_to);
			return -1;
		}

		remaining_size -= actually_read;
	}

	close(fd_rand);
	close(fd_to);

	return 0;
}

int main() {
	mkdir("../a", S_IRWXU);
	mkdir("../a/b", S_IRWXU);

	create_text_file("../a/b/cat.txt", "catts");
	mkdir("../a/c", S_IRWXU);
	create_empty_file("../a/c/dog.txt");

	symlink("../a/c", "../a/d");

	mkdir("../a/e", S_IRWXU);
	link("../a/b/cat.txt", "../a/e/f.txt");

	create_random_file("../a/e/g.bin", 500);
	create_zero_file("../a/e/h.bin", 35);
}