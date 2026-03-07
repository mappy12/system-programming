#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

int create_text_file(const char* path, const char* content) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
	if (fd == -1) exit(-1);

	size_t len = strlen(content);

	ssize_t written_bites = write(fd, content, len);
	if (written_bites != (ssize_t)len) {
		close(fd);
		errno = (written_bites == -1) ? errno : EIO;
		exit(-1);
	}

	close(fd);
	return 0;
}

int create_empty_file(const char* path) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

	if (fd == -1) {
		exit(-1);
	}

	close(fd);
	return 0;
}

int create_zero_file(const char* path, size_t size) {
	int fd = open(path, O_CREAT | O_WRONLY, S_IRWXU);
	if (fd == -1) exit(-1);

	if (ftruncate(fd, (off_t)size) == -1) {
		close(fd);
		exit(-1);
	}

	close(fd);
	return 0;
}

int create_random_file(const char* path, size_t size) {
	int fd_to = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);

	if (fd_to == -1) {
		exit(-1);
	}

	int fd_rand = open("/dev/urandom", O_RDONLY);
	if (fd_rand == -1) {
		close(fd_to);
		exit(-1);
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
			exit(-1);
		}

		if (write(fd_to, buffer, actually_read) != actually_read) {
			close(fd_rand);
			close(fd_to);
			exit(-1);
		}

		remaining_size -= actually_read;
	}

	close(fd_rand);
	close(fd_to);

	return 0;
}

void remove_dir() {
	if (unlink("breaking-bad/Gustaw's cartel/Gustaw.bin") == -1)
		perror("UNLINK ERROR: breaking-bad/Gustaw's cartel/Gustaw.bin");
	if (unlink("breaking-bad/Gustaw's cartel/Pinkman.bin") == -1)
		perror("UNLINK ERROR: breaking-bad/Gustaw's cartel/Pinkman.bin");
	if (rmdir("breaking-bad/Gustaw's cartel") == -1)
		perror("RMDIR ERROR: ");
	if (unlink("breaking-bad/Heisenberg's group/Pinkman.bin") == -1)
		perror("UNLINK ERROR: breaking-bad/Heisenberg's group/Pinkman.bin ");
	if (unlink("breaking-bad/Heisenberg's group/Heisenberg.txt") == -1)
		perror("UNLINK ERROR: breaking-bad/Heisenberg's group/Heisenberg.txt");
	if (rmdir("breaking-bad/Heisenberg's group") == -1)
		perror("RMDIR ERROR: breaking-bad/Heisenberg's group");
	if (unlink("breaking-bad/Heisenberg_link.txt") == -1)
		perror("UNLINK ERROR: breaking-bad/Heisenberg_link.txt");
	if (rmdir("breaking-bad") == -1)
		perror("RMDIR ERROR: breaking-bad");
}

int main() {

	mkdir("breaking-bad", S_IRWXU);

	mkdir("breaking-bad/Heisenberg's group", S_IRWXU);
	create_text_file("breaking-bad/Heisenberg's group/Heisenberg.txt", "You're goddamn right");
	create_random_file("breaking-bad/Heisenberg's group/Pinkman.bin", 1000);

	mkdir("breaking-bad/Gustaw's cartel", S_IRWXU);
	create_zero_file("breaking-bad/Gustaw's cartel/Gustaw.bin", 300);
	symlink("../Heisenberg's group/Pinkman.bin", "breaking-bad/Gustaw's cartel/Pinkman.bin");
	link("breaking-bad/Heisenberg's group/Heisenberg.txt", "breaking-bad/Heisenberg_link.txt");

	remove_dir();
}