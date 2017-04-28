/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
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
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <jailhouse.h>

#define JAILHOUSE_EXEC_DIR	LIBEXECDIR "/jailhouse"
#define JAILHOUSE_DEVICE	"/dev/jailhouse"
#define JAILHOUSE_CELLS		"/sys/devices/jailhouse/cells/"

enum shutdown_load_mode {LOAD, SHUTDOWN};

struct extension {
	char *cmd, *subcmd, *help;
};

struct jailhouse_cell_info {
	struct jailhouse_cell_id id;
	char *state;
	char *cpus_assigned_list;
	char *cpus_failed_list;
};

static const struct extension extensions[] = {
	{ "cell", "linux", "CELLCONFIG KERNEL [-i | --initrd FILE]\n"
	  "              [-c | --cmdline \"STRING\"] "
					"[-w | --write-params FILE]" },
	{ "cell", "stats", "{ ID | [--name] NAME }" },
	{ "config", "create", "[-h] [-g] [-r ROOT] "
	  "[--mem-inmates MEM_INMATES]\n"
	  "                 [--mem-hv MEM_HV] FILE" },
	{ "config", "collect", "FILE.TAR" },
	{ "hardware", "check", "SYSCONFIG" },
	{ NULL }
};

static void __attribute__((noreturn)) help(char *prog, int exit_status)
{
	const struct extension *ext;

	printf("Usage: %s { COMMAND | --help | --version }\n"
	       "\nAvailable commands:\n"
	       "   enable SYSCONFIG\n"
	       "   disable\n"
	       "   console [-f | --follow]\n"
	       "   cell create CELLCONFIG\n"
	       "   cell list\n"
	       "   cell load { ID | [--name] NAME } "
				"{ IMAGE | { -s | --string } \"STRING\" }\n"
	       "             [-a | --address ADDRESS] ...\n"
	       "   cell start { ID | [--name] NAME }\n"
	       "   cell shutdown { ID | [--name] NAME }\n"
	       "   cell destroy { ID | [--name] NAME }\n",
	       basename(prog));
	for (ext = extensions; ext->cmd; ext++)
		printf("   %s %s %s\n", ext->cmd, ext->subcmd, ext->help);

	exit(exit_status);
}

static void call_extension_script(const char *cmd, int argc, char *argv[])
{
	const struct extension *ext;
	char new_path[PATH_MAX];
	char script[64];

	if (argc < 3)
		return;

	for (ext = extensions; ext->cmd; ext++) {
		if (strcmp(ext->cmd, cmd) != 0 ||
		    strcmp(ext->subcmd, argv[2]) != 0)
			continue;

		snprintf(new_path, sizeof(new_path), "PATH=%s:%s:%s",
			dirname(argv[0]), JAILHOUSE_EXEC_DIR,
			getenv("PATH") ? : "");
		putenv(new_path);

		snprintf(script, sizeof(script), "jailhouse-%s-%s",
			 cmd, ext->subcmd);
		execvp(script, &argv[2]);

		perror("execvp");
		exit(1);
	}
}

static int open_dev()
{
	int fd;

	fd = open(JAILHOUSE_DEVICE, O_RDWR);
	if (fd < 0) {
		perror("opening " JAILHOUSE_DEVICE);
		exit(1);
	}
	return fd;
}

static void *read_string(const char *string, size_t *size)
{
	void *buffer;

	*size = strlen(string) + 1;

	buffer = strdup(string);
	if (!buffer) {
		fprintf(stderr, "insufficient memory\n");
		exit(1);
	}

	return buffer;
}

static void *read_file(const char *name, size_t *size)
{
	struct stat stat;
	ssize_t result;
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

	result = read(fd, buffer, stat.st_size);
	if (result < 0) {
		fprintf(stderr, "reading %s: %s\n", name, strerror(errno));
		exit(1);
	}

	close(fd);

	if (size)
		*size = (size_t)result;

	return buffer;
}

static char *read_sysfs_cell_string(const unsigned int id, const char *entry)
{
	char *ret, buffer[128];
	size_t size;

	snprintf(buffer, sizeof(buffer), JAILHOUSE_CELLS "%u/%s", id, entry);
	ret = read_file(buffer, &size);

	/* entries in /sys/devices/jailhouse/cells must not be empty */
	if (size == 0) {
		snprintf(buffer, sizeof(buffer),
			 "reading " JAILHOUSE_CELLS "%u/%s", id, entry);
		perror(buffer);
		exit(1);
	}

	/* chop trailing linefeeds and enforce the string to be
	 * null-terminated */
	if (ret[size-1] != '\n') {
		ret = realloc(ret, ++size);
		if (ret == NULL) {
			fprintf(stderr, "insufficient memory\n");
			exit(1);
		}
	}
	ret[size-1] = 0;

	return ret;
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
	free((void *)(unsigned long)cell_create.config_address);

	return err;
}

static int parse_cell_id(struct jailhouse_cell_id *cell_id, int argc,
			 char *argv[])
{
	bool use_name = false;
	int arg_pos = 0;
	char *endp;

	memset(cell_id, 0, sizeof(*cell_id));

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
		/* cell_id is initialized with zeros, so leaving out the last
		 * byte ensures that the string is always terminated. */
		strncpy(cell_id->name, argv[arg_pos],
			sizeof(cell_id->name) - 1);
	}

	return arg_pos + 1;
}

static bool match_opt(const char *argv, const char *short_opt,
		      const char *long_opt)
{
	return strcmp(argv, short_opt) == 0 ||
		strcmp(argv, long_opt) == 0;
}

static struct jailhouse_cell_info *get_cell_info(const unsigned int id)
{
	struct jailhouse_cell_info *cinfo;
	char *tmp;

	cinfo = malloc(sizeof(struct jailhouse_cell_info));
	if (cinfo == NULL) {
		fprintf(stderr, "insufficient memory\n");
		exit(1);
	}

	/* set cell id */
	cinfo->id.id = id;

	/* get cell name */
	tmp = read_sysfs_cell_string(id, "name");
	strncpy(cinfo->id.name, tmp, JAILHOUSE_CELL_ID_NAMELEN);
	cinfo->id.name[JAILHOUSE_CELL_ID_NAMELEN] = 0;
	free(tmp);

	/* get cell state */
	cinfo->state = read_sysfs_cell_string(id, "state");

	/* get assigned cpu list */
	cinfo->cpus_assigned_list =
		read_sysfs_cell_string(id, "cpus_assigned_list");

	/* get failed cpu list */
	cinfo->cpus_failed_list = read_sysfs_cell_string(id, "cpus_failed_list");

	return cinfo;
}

static void cell_info_free(struct jailhouse_cell_info *cinfo)
{
	free(cinfo->state);
	free(cinfo->cpus_assigned_list);
	free(cinfo->cpus_failed_list);
	free(cinfo);
}

static int cell_match(const struct dirent *dirent)
{
	return dirent->d_name[0] != '.';
}

static int cell_list(int argc, char *argv[])
{
	struct dirent **namelist;
	struct jailhouse_cell_info *cinfo;
	unsigned int id;
	int i, num_entries;
	(void)argv;

	if (argc != 3)
		help(argv[0], 1);

	num_entries = scandir(JAILHOUSE_CELLS, &namelist, cell_match, alphasort);
	if (num_entries == -1) {
		/* Silently return if kernel module is not loaded */
		if (errno == ENOENT)
			return 0;

		perror("scandir");
		return -1;
	}

	if (num_entries > 0)
		printf("%-8s%-24s%-16s%-24s%-24s\n",
		       "ID", "Name", "State", "Assigned CPUs", "Failed CPUs");
	for (i = 0; i < num_entries; i++) {
		id = (unsigned int)strtoul(namelist[i]->d_name, NULL, 10);

		cinfo = get_cell_info(id);
		printf("%-8d%-24s%-16s%-24s%-24s\n", cinfo->id.id, cinfo->id.name,
		       cinfo->state, cinfo->cpus_assigned_list, cinfo->cpus_failed_list);
		cell_info_free(cinfo);
		free(namelist[i]);
	}

	free(namelist);
	return 0;
}

static int cell_shutdown_load(int argc, char *argv[],
			      enum shutdown_load_mode mode)
{
	struct jailhouse_preload_image *image;
	struct jailhouse_cell_load *cell_load;
	struct jailhouse_cell_id cell_id;
	int err, fd, id_args, arg_num;
	unsigned int images, n;
	size_t size;
	char *endp;

	id_args = parse_cell_id(&cell_id, argc - 3, &argv[3]);
	arg_num = 3 + id_args;
	if (id_args == 0 || (mode == SHUTDOWN && arg_num != argc) ||
	    (mode == LOAD && arg_num == argc))
		help(argv[0], 1);

	images = 0;
	while (arg_num < argc) {
		if (match_opt(argv[arg_num], "-s", "--string")) {
			if (arg_num + 1 >= argc)
				help(argv[0], 1);
			arg_num++;
		}

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
		if (match_opt(argv[arg_num], "-s", "--string")) {
			arg_num++;
			image->source_address =
				(unsigned long)read_string(argv[arg_num++],
							   &size);
		} else {
			image->source_address =
				(unsigned long)read_file(argv[arg_num++],
							 &size);
		}
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

static int cell_management(int argc, char *argv[])
{
	int err;

	if (argc < 3)
		help(argv[0], 1);

	if (strcmp(argv[2], "create") == 0) {
		err = cell_create(argc, argv);
	} else if (strcmp(argv[2], "list") == 0) {
		err = cell_list(argc, argv);
	} else if (strcmp(argv[2], "load") == 0) {
		err = cell_shutdown_load(argc, argv, LOAD);
	} else if (strcmp(argv[2], "start") == 0) {
		err = cell_simple_cmd(argc, argv, JAILHOUSE_CELL_START);
	} else if (strcmp(argv[2], "shutdown") == 0) {
		err = cell_shutdown_load(argc, argv, SHUTDOWN);
	} else if (strcmp(argv[2], "destroy") == 0) {
		err = cell_simple_cmd(argc, argv, JAILHOUSE_CELL_DESTROY);
	} else {
		call_extension_script("cell", argc, argv);
		help(argv[0], 1);
	}

	return err;
}

static int console(int argc, char *argv[])
{
	bool non_block = true;
	char buffer[128];
	ssize_t ret;
	int fd;

	if (argc == 3) {
		if (match_opt(argv[2], "-f", "--follow"))
			non_block = false;
		else
			help(argv[0], 1);
	}

	fd = open_dev();

	if (non_block) {
		ret = fcntl(fd, F_SETFL, O_NONBLOCK);
		if (ret < 0) {
			perror("fcntl(set O_NONBLOCK)");
			goto out;
		}
	}

	do {
		ret = read(fd, buffer, sizeof(buffer));
		if (ret < 0) {
			perror("read(console)");
			break;
		}
		ret = write(STDOUT_FILENO, buffer, ret);
	} while (ret > 0);

out:
	close(fd);

	return ret;
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
	} else if (strcmp(argv[1], "console") == 0) {
		err = console(argc, argv);
	} else if (strcmp(argv[1], "config") == 0 ||
		   strcmp(argv[1], "hardware") == 0) {
		call_extension_script(argv[1], argc, argv);
		help(argv[0], 1);
	} else if (strcmp(argv[1], "--version") == 0) {
		printf("Jailhouse management tool %s\n", JAILHOUSE_VERSION);
		return 0;
	} else if (strcmp(argv[1], "--help") == 0) {
		help(argv[0], 0);
	} else {
		help(argv[0], 1);
	}

	return err ? 1 : 0;
}
