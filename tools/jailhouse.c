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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <jailhouse.h>

enum shutdown_load_mode {LOAD, SHUTDOWN};

static void __attribute__((noreturn))
help(const char *progname, int exit_status)
{
	printf("%s { COMMAND | --help }\n"
	       "\nAvailable commands:\n"
	       "   enable SYSCONFIG\n"
	       "   disable\n"
	       "   cell create CELLCONFIG\n"
	       "   cell load { ID | [--name] NAME } IMAGE "
			"[ -a | --address ADDRESS] ...\n"
	       "   cell start { ID | [--name] NAME }\n"
	       "   cell shutdown { ID | [--name] NAME }\n"
	       "   cell destroy { ID | [--name] NAME }\n"
	       "   cell list\n",
	       progname);
	exit(exit_status);
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

	if (argc != 3)
		help(argv[0], 1);

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
	struct jailhouse_cell_create cell_create;
	size_t size;
	int err, fd;

	if (argc != 4)
		help(argv[0], 1);

	cell_create.config_address = (unsigned long)read_file(argv[3], &size);
	cell_create.config_size = size;

	fd = open_dev();

	err = ioctl(fd, JAILHOUSE_CELL_CREATE, &cell_create);
	if (err)
		perror("JAILHOUSE_CELL_CREATE");

	close(fd);

	return err;
}

static int parse_cell_id(struct jailhouse_cell_id *cell_id, int argc,
			 char *argv[])
{
	bool use_name = false;
	int arg_pos = 0;
	char *endp;

	if (argc < 1)
		return 0;

	if (strcmp(argv[0], "--name") == 0) {
		if (argc < 2)
			return 0;
		arg_pos++;
		use_name = true;
	} else {
		errno = 0;
		cell_id->id = strtoll(argv[0], &endp, 0);
		if (errno != 0 || *endp != 0 || cell_id->id < 0)
			use_name = true;
	}

	if (use_name) {
		cell_id->id = JAILHOUSE_CELL_ID_UNUSED;
		strncpy(cell_id->name, argv[arg_pos], sizeof(cell_id->name));
		cell_id->name[sizeof(cell_id->name) - 1] = 0;
	}

	return arg_pos + 1;
}

static bool match_opt(const char *argv, const char *short_opt,
		      const char *long_opt)
{
	return strcmp(argv, short_opt) == 0 ||
		strcmp(argv, long_opt) == 0;
}

static int cell_shutdown_load(int argc, char *argv[],
			      enum shutdown_load_mode mode)
{
	unsigned int images, id_args, arg_num, n;
	struct jailhouse_preload_image *image;
	struct jailhouse_cell_load *cell_load;
	struct jailhouse_cell_id cell_id;
	size_t size;
	int err, fd;
	char *endp;

	id_args = parse_cell_id(&cell_id, argc - 3, &argv[3]);
	arg_num = 3 + id_args;
	if (id_args == 0 || (mode == SHUTDOWN && arg_num != argc) ||
	    (mode == LOAD && arg_num == argc))
		help(argv[0], 1);

	images = 0;
	while (arg_num < argc) {
		images++;
		arg_num++;

		if (arg_num < argc &&
		    match_opt(argv[arg_num], "-a", "--address")) {
			if (arg_num + 1 >= argc)
				help(argv[0], 1);
			arg_num += 2;
		}
	}

	cell_load = malloc(sizeof(*cell_load) + sizeof(*image) * images);
	if (!cell_load) {
		fprintf(stderr, "insufficient memory\n");
		exit(1);
	}
	cell_load->cell_id = cell_id;
	cell_load->num_preload_images = images;

	arg_num = 3 + id_args;

	for (n = 0, image = cell_load->image; n < images; n++, image++) {
		image->source_address =
			(unsigned long)read_file(argv[arg_num++], &size);
		image->size = size;
		image->target_address = 0;

		if (arg_num < argc &&
		    match_opt(argv[arg_num], "-a", "--address")) {
			errno = 0;
			image->target_address =
				strtoll(argv[arg_num + 1], &endp, 0);
			if (errno != 0 || *endp != 0)
				help(argv[0], 1);
			arg_num += 2;
		}
		image++;
	}

	fd = open_dev();

	err = ioctl(fd, JAILHOUSE_CELL_LOAD, cell_load);
	if (err)
		perror("JAILHOUSE_CELL_LOAD");

	close(fd);
	for (n = 0, image = cell_load->image; n < images; n++, image++)
		free((void *)(unsigned long)image->source_address);
	free(cell_load);

	return err;
}

static int cell_simple_cmd(int argc, char *argv[], unsigned int command)
{
	struct jailhouse_cell_id cell_id;
	int id_args, err, fd;

	id_args = parse_cell_id(&cell_id, argc - 3, &argv[3]);
	if (id_args == 0 || 3 + id_args != argc)
		help(argv[0], 1);

	fd = open_dev();

	err = ioctl(fd, command, &cell_id);
	if (err)
		perror(command == JAILHOUSE_CELL_START ?
		       "JAILHOUSE_CELL_START" :
		       command == JAILHOUSE_CELL_DESTROY ?
		       "JAILHOUSE_CELL_DESTROY" :
		       "<unknown command>");

	close(fd);

	return err;
}

static int cell_list(int argc, char *argv[])
{
	static const char *cell_list_cmd = "jailhouse-cell-list";
	char new_path[PATH_MAX];
	int err;

	if (argc != 3)
		help(argv[0], 1);

	snprintf(new_path, sizeof(new_path), "PATH=%s:%s",
		 getenv("PATH") ? : "", dirname(argv[0]));
	putenv(new_path);

	err = execvp(cell_list_cmd, argv);

	perror(cell_list_cmd);
	return err;
}

static int cell_management(int argc, char *argv[])
{
	int err;

	if (argc < 3)
		help(argv[0], 1);

	if (strcmp(argv[2], "create") == 0)
		err = cell_create(argc, argv);
	else if (strcmp(argv[2], "load") == 0)
		err = cell_shutdown_load(argc, argv, LOAD);
	else if (strcmp(argv[2], "start") == 0)
		err = cell_simple_cmd(argc, argv, JAILHOUSE_CELL_START);
	else if (strcmp(argv[2], "shutdown") == 0)
		err = cell_shutdown_load(argc, argv, SHUTDOWN);
	else if (strcmp(argv[2], "destroy") == 0)
		err = cell_simple_cmd(argc, argv, JAILHOUSE_CELL_DESTROY);
	else if (strcmp(argv[2], "list") == 0)
		err = cell_list(argc, argv);
	else
		help(argv[0], 1);

	return err;
}

int main(int argc, char *argv[])
{
	int fd;
	int err;

	if (argc < 2)
		help(argv[0], 1);

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
	} else if (strcmp(argv[1], "--help") == 0) {
		help(argv[0], 0);
	} else
		help(argv[0], 1);

	return err ? 1 : 0;
}
