/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2019-2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <errno.h>
#include <error.h>
#include <libgen.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/signalfd.h>

struct ivshm_regs {
	uint32_t id;
	uint32_t max_peers;
	uint32_t int_control;
	uint32_t doorbell;
	uint32_t state;
};

static volatile uint32_t *state, *rw, *in, *out;
static uint32_t id, int_count;

static inline uint32_t mmio_read32(void *address)
{
        return *(volatile uint32_t *)address;
}

static inline void mmio_write32(void *address, uint32_t value)
{
        *(volatile uint32_t *)address = value;
}

static void print_shmem(void)
{
	printf("state[0] = %d\n", state[0]);
	printf("state[1] = %d\n", state[1]);
	printf("state[2] = %d\n", state[2]);
	printf("rw[0] = %d\n", rw[0]);
	printf("rw[1] = %d\n", rw[1]);
	printf("rw[2] = %d\n", rw[2]);
	printf("in@0x0000 = %d\n", in[0/4]);
	printf("in@0x2000 = %d\n", in[0x2000/4]);
	printf("in@0x4000 = %d\n", in[0x4000/4]);
}

int main(int argc, char *argv[])
{
	char sysfs_path[64];
	struct ivshm_regs *regs;
	uint32_t int_no, target = 0;
	struct signalfd_siginfo siginfo;
	struct pollfd fds[2];
	sigset_t sigset;
	char *path;
	int has_msix;
	int ret;

	if (argc < 2)
		path = strdup("/dev/uio0");
	else
		path = strdup(argv[1]);
	fds[0].fd = open(path, O_RDWR);
	if (fds[0].fd < 0)
		error(1, errno, "open(%s)", path);
	fds[0].events = POLLIN;

	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/class/uio/%s/device/msi_irqs", basename(path));
	has_msix = access(sysfs_path, R_OK) == 0;

	regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fds[0].fd, 0);
	if (regs == MAP_FAILED)
		error(1, errno, "mmap(regs)");

	id = mmio_read32(&regs->id);
	printf("ID = %d\n", id);

	state = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fds[0].fd, 4096 * 1);
	if (state == MAP_FAILED)
		error(1, errno, "mmap(state)");

	rw = mmap(NULL, 4096 * 9, PROT_READ | PROT_WRITE, MAP_SHARED,
		  fds[0].fd, 4096 * 2);
	if (rw == MAP_FAILED)
		error(1, errno, "mmap(rw)");

	in = mmap(NULL, 4096 * 6, PROT_READ, MAP_SHARED, fds[0].fd, 4096 * 3);
	if (in == MAP_FAILED)
		error(1, errno, "mmap(in)");

	out = mmap(NULL, 4096 * 2, PROT_READ | PROT_WRITE, MAP_SHARED,
		   fds[0].fd, 4096 * 4);
	if (out == MAP_FAILED)
		error(1, errno, "mmap(out)");

	mmio_write32(&regs->state, id + 1);
	rw[id] = 0;
	out[0] = 0;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	fds[1].fd = signalfd(-1, &sigset, 0);
	if (fds[1].fd < 0)
		error(1, errno, "signalfd");
	fds[1].events = POLLIN;

	mmio_write32(&regs->int_control, 1);
	alarm(1);

	print_shmem();

	while (1) {
		ret = poll(fds, 2, -1);
		if (ret < 0)
			error(1, errno, "poll");

		if (fds[0].revents & POLLIN) {
			ret = read(fds[0].fd, &int_count, sizeof(int_count));
			if (ret != sizeof(int_count))
				error(1, errno, "read(uio)");

			rw[id] = int_count;
			out[0] = int_count * 10;
			printf("\nInterrupt #%d\n", int_count);
			print_shmem();

			mmio_write32(&regs->int_control, 1);
		}
		if (fds[1].revents & POLLIN) {
			ret = read(fds[1].fd, &siginfo, sizeof(siginfo));
			if (ret != sizeof(siginfo))
				error(1, errno, "read(sigfd)");

			int_no = has_msix ? (id + 1) : 0;
			target = (id + 1) % 3;
			printf("\nSending interrupt %d to peer %d\n",
			       int_no, target);
			mmio_write32(&regs->doorbell, int_no | (target << 16));

			alarm(1);
		}
	}
}
