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

#include <inmate.h>

#define MSR_SMI_COUNT		0x34

#define POLLUTE_CACHE_SIZE	(512 * 1024)

#define APIC_TIMER_VECTOR	32

/*
 * Enables blinking LED
 * SIMATIC IPC127E:     register 0xd0c506a8, pin 0
 */
static void *led_reg;
static unsigned int led_pin;

static unsigned long expected_time;
static unsigned long min = -1, max;
static bool has_smi_count;
static u32 initial_smis;

static const unsigned int smi_count_models[] = {
	0x37, 0x4a, 0x4d, 0x5a, 0x5d, 0x5c, 0x7a,	/* Silvermont */
	0x1a, 0x1e, 0x1f, 0x2e,				/* Nehalem */
	0x2a, 0x2d,					/* Sandy Bridge */
	0x57, 0x85,					/* Xeon Phi */
	0
};

static bool cpu_has_smi_count(void)
{
	unsigned int family, model, smi_count_model, n = 0;
	unsigned long eax;

	asm volatile("cpuid" : "=a" (eax) : "a" (1)
		: "rbx", "rcx", "rdx", "memory");
	family = ((eax & 0xf00) >> 8) | ((eax & 0xff00000) >> 16);
	model = ((eax & 0xf0) >> 4) | ((eax & 0xf0000) >> 12);
	if (family == 0x6) {
		do {
			smi_count_model = smi_count_models[n++];
			if (model == smi_count_model)
				return true;
		} while (smi_count_model != 0);
	}
	return false;
}

static void irq_handler(unsigned int irq)
{
	unsigned long delta;
	u32 smis;

	if (irq != APIC_TIMER_VECTOR)
		return;

	delta = tsc_read_ns() - expected_time;
	if (delta < min)
		min = delta;
	if (delta > max)
		max = delta;
	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns",
	       delta, min, max);
	if (has_smi_count) {
		smis = (u32)read_msr(MSR_SMI_COUNT);
		if (smis != initial_smis)
			printk(", SMIs: %d", smis - initial_smis);
	}
	printk("\n");

	if (led_reg)
		mmio_write32(led_reg, mmio_read32(led_reg) ^ (1 << led_pin));

	expected_time += 100 * NS_PER_MSEC;
	apic_timer_set(expected_time - tsc_read_ns());
}

static void init_apic(void)
{
	unsigned long apic_freq_khz;

	irq_init(irq_handler);

	apic_freq_khz = apic_timer_init(APIC_TIMER_VECTOR);
	printk("Calibrated APIC frequency: %lu kHz\n", apic_freq_khz);

	expected_time = tsc_read_ns() + NS_PER_MSEC;
	apic_timer_set(NS_PER_MSEC);

	asm volatile("sti");
}

static void pollute_cache(char *mem)
{
	unsigned long cpu_cache_line_size, ebx;
	unsigned long n;

	asm volatile("cpuid" : "=b" (ebx) : "a" (1)
		: "rcx", "rdx", "memory");
	cpu_cache_line_size = (ebx & 0xff00) >> 5;

	for (n = 0; n < POLLUTE_CACHE_SIZE; n += cpu_cache_line_size)
		mem[n] ^= 0xAA;
}

void inmate_main(void)
{
	bool allow_terminate = false;
	bool terminate = false;
	unsigned long tsc_freq;
	bool cache_pollution;
	char *mem;

	comm_region->cell_state = JAILHOUSE_CELL_RUNNING_LOCKED;

	cache_pollution = cmdline_parse_bool("pollute-cache", false);
	if (cache_pollution) {
		mem = alloc(PAGE_SIZE, PAGE_SIZE);
		printk("Cache pollution enabled\n");
	}

	has_smi_count = cpu_has_smi_count();
	if (has_smi_count) {
		initial_smis = (u32)read_msr(MSR_SMI_COUNT);
		printk("Initial number of SMIs: %d\n", initial_smis);
	}

	tsc_freq = tsc_init();
	printk("Calibrated TSC frequency: %lu.%03lu kHz\n", tsc_freq / 1000,
	       tsc_freq % 1000);

	init_apic();

	led_reg = (void *)(unsigned long)cmdline_parse_int("led-reg", 0);
	led_pin = cmdline_parse_int("led-pin", 0);

	if (led_reg)
		map_range(led_reg, 4, MAP_UNCACHED);

	while (!terminate) {
		cpu_relax();

		if (cache_pollution)
			pollute_cache(mem);

		switch (comm_region->msg_to_cell) {
		case JAILHOUSE_MSG_SHUTDOWN_REQUEST:
			if (!allow_terminate) {
				printk("Rejecting first shutdown request - "
				       "try again!\n");
				jailhouse_send_reply_from_cell(comm_region,
						JAILHOUSE_MSG_REQUEST_DENIED);
				allow_terminate = true;
			} else
				terminate = true;
			break;
		case JAILHOUSE_MSG_NONE:
			break;
		default:
			jailhouse_send_reply_from_cell(comm_region,
					JAILHOUSE_MSG_UNKNOWN);
			break;
		}
	}

	printk("Stopped APIC demo\n");
	comm_region->cell_state = JAILHOUSE_CELL_SHUT_DOWN;
}
