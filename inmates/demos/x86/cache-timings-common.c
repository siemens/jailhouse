/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2020
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define ROUNDS	(10 * 1000 * 1000)

union tscval {
	struct {
		u32 lo;
		u32 hi;
	} __attribute__((packed));
	u64 val;
} __attribute__((packed));

static u32 victim;

static inline void clflush(void *addr)
{
	asm volatile("clflush %0\t\n"
		     "mfence\t\n"
		     "lfence\t\n" : "+m" (*(volatile char *)addr));
}

#define MEASUREMENT_OVERHEAD	"nop\t\n"
#define MEASUREMENT_COMMAND	"mov (%%rbx), %%ebx\t\n"
#define DECLARE_MEASUREMENT(name, flush, meas) \
	static inline u64 measure_##name(u32 *victim)			\
	{								\
		union tscval before, after;				\
									\
		if (flush)						\
			clflush(victim);				\
		asm volatile("mov %4, %%rbx\t\n"			\
			     "lfence\t\n"				\
			     "rdtsc\t\n"				\
			     "lfence\t\n"				\
									\
			     meas					\
									\
			     "mov %%eax, %%ebx\t\n"			\
			     "mov %%edx, %%ecx\t\n"			\
			     "lfence\t\n"				\
			     "rdtsc\t\n"				\
			     "lfence\t\n"				\
			     "mov %%ebx, %0\t\n"			\
			     "mov %%ecx, %1\t\n"			\
			     "mov %%eax, %2\t\n"			\
			     "mov %%edx, %3\t\n"			\
			     : "=m"(before.lo), "=m" (before.hi),	\
			       "=m" (after.lo), "=m" (after.hi)		\
			     : "m" (victim)				\
			     : "eax", "rbx", "ecx", "edx");		\
		return after.val - before.val;				\
	}

DECLARE_MEASUREMENT(overhead, false, MEASUREMENT_OVERHEAD)
DECLARE_MEASUREMENT(cached, false, MEASUREMENT_COMMAND)
DECLARE_MEASUREMENT(uncached, true, MEASUREMENT_COMMAND)

static inline u64 avg_measurement(u64 (*meas)(u32*), u32 *victim,
				  unsigned int rounds, u64 overhead)
{
	u64 cycles = 0;
	unsigned int i;

	for (i = 0; i < rounds; i++)
		cycles += meas(victim) - overhead;
	return cycles / rounds;
}

void inmate_main(void)
{
	u64 cycles, overhead;

	printk("Measurement rounds: %u\n", ROUNDS);
	printk("Determining measurement overhead...\n");
	overhead = avg_measurement(measure_overhead, &victim, ROUNDS, 0);
	printk("  -> Average measurement overhead: %llu cycles\n", overhead);

	printk("Measuring uncached memory access...\n");
	cycles = avg_measurement(measure_uncached, &victim, ROUNDS, overhead);
	printk("  -> Average uncached memory access: %llu cycles\n", cycles);

	printk("Measuring cached memory access...\n");
	cycles = avg_measurement(measure_cached, &victim, ROUNDS, overhead);
	printk("  -> Average cached memory access: %llu cycles\n", cycles);
}
