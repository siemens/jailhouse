/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <jailhouse.h>

static void help(const char *progname)
{
	printf("%s <command> <args>\n"
	       "\nAvailable commands:\n"
	       "   enable CONFIGFILE\n"
	       "   disable\n"
	       "   cell create CONFIGFILE PRELOADIMAGE [-l ADDRESS]\n"
	       "   cell destroy NAME\n",
	       progname);
}

static int open_dev()
{
	int fd;

	fd = open("/dev/jailhouse", O_RDWR);
	if (fd < 0) {
		perror("opening /dev/jailhouse");
		exit(1);
	}
	return fd;
}

static void *read_file(const char *name, size_t *size)
{
	struct stat stat;
	void *buffer;
	int fd;

	fd = open(name, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "opening %s: %s\n", name, strerror(errno));
		exit(1);
	}

	if (fstat(fd, &stat) < 0) {
		perror("fstat");
		exit(1);
	}

	buffer = malloc(stat.st_size);
	if (!buffer) {
		fprintf(stderr, "insufficient memory\n");
		exit(1);
	}

	if (read(fd, buffer, stat.st_size) < stat.st_size) {
		fprintf(stderr, "reading %s: %s\n", name, strerror(errno));
		exit(1);
	}

	close(fd);

	if (size)
		*size = stat.st_size;

	return buffer;
}

static int enable(int argc, char *argv[])
{
	void *config;
	int err, fd;

	if (argc != 3) {
		help(argv[0]);
		exit(1);
	}

	config = read_file(argv[2], NULL);

	fd = open_dev();

	err = ioctl(fd, JAILHOUSE_ENABLE, config);
	if (err)
		perror("JAILHOUSE_ENABLE");

	close(fd);
	free(config);

	return err;
}

static int cell_create(int argc, char *argv[])
{
	struct {
		struct jailhouse_new_cell cell;
		struct jailhouse_preload_image image;
	} params;
	struct jailhouse_new_cell *cell = &params.cell;
	struct jailhouse_preload_image *image = params.cell.image;
	size_t size;
	int err, fd;
	char *endp;

	if (argc != 5 && argc != 7) {
		help(argv[0]);
		exit(1);
	}

	cell->config_address = (unsigned long)read_file(argv[3], &size);
	cell->config_size = size;
	cell->num_preload_images = 1;

	image->source_address = (unsigned long)read_file(argv[4], &size);
	image->size = size;
	image->target_address = 0;

	if (argc == 7) {
		errno = 0;
		image->target_address = strtoll(argv[6], &endp, 0);
		if (errno != 0 || *endp != 0 || strcmp(argv[5], "-l") != 0) {
			help(argv[0]);
			exit(1);
		}
	}

	fd = open_dev();

	err = ioctl(fd, JAILHOUSE_CELL_CREATE, &params);
	if (err)
		perror("JAILHOUSE_CELL_CREATE");

	close(fd);
	free((void *)(unsigned long)cell->config_address);
	free((void *)(unsigned long)image->source_address);

	return err;
}

static int cell_destroy(int argc, char *argv[])
{
	int err, fd;

	if (argc != 4) {
		help(argv[0]);
		exit(1);
	}

	fd = open_dev();

	err = ioctl(fd, JAILHOUSE_CELL_DESTROY, argv[3]);
	if (err)
		perror("JAILHOUSE_CELL_DESTROY");

	close(fd);

	return err;
}

static int cell_management(int argc, char *argv[])
{
	int err;

	if (argc < 3) {
		help(argv[0]);
		exit(1);
	}

	if (strcmp(argv[2], "create") == 0)
		err = cell_create(argc, argv);
	else if (strcmp(argv[2], "destroy") == 0)
		err = cell_destroy(argc, argv);
	else {
		help(argv[0]);
		exit(1);
	}

	return err;
}

int main(int argc, char *argv[])
{
	int fd;
	int err;

	if (argc < 2) {
		help(argv[0]);
		exit(1);
	}

	if (strcmp(argv[1], "enable") == 0) {
		err = enable(argc, argv);
	} else if (strcmp(argv[1], "disable") == 0) {
		fd = open_dev();
		err = ioctl(fd, JAILHOUSE_DISABLE);
		if (err)
			perror("JAILHOUSE_DISABLE");
		close(fd);
	} else if (strcmp(argv[1], "cell") == 0) {
		err = cell_management(argc, argv);
	} else {
		help(argv[0]);
		exit(1);
	}

	return err ? 1 : 0;
}
