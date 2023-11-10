/* reference: https://github.com/a-darwish/cuteOS/blob/35d59938100d136275ede18b9756d238292455fa/dev/pit.c
 *
 */


#include <entry.h>

#define PIT_CLOCK_RATE	1193182ul	/* Hz (ticks per second) */
#define cpu_pause()						\
	asm volatile ("pause":::"memory");

/*
 * Extract a PIT-related bit from port 0x61
 */
enum {
	PIT_GATE2   = 0x1,		/* bit 0 - PIT's GATE-2 input */
	PIT_SPEAKER = 0x2,		/* bit 1 - enable/disable speaker */
	PIT_OUT2    = 0x20,		/* bit 5 - PIT's OUT-2 */
};

/*
 * The PIT's system interface: an array of peripheral I/O ports.
 * The peripherals chip translates accessing below ports to the
 * right PIT's A0 and A1 address pins, and RD/WR combinations.
 */
enum {
	PIT_COUNTER0 =	0x40,		/* read/write Counter 0 latch */
	PIT_COUNTER1 =	0x41,		/* read/write Counter 1 latch */
	PIT_COUNTER2 =	0x42,		/* read/write Counter 2 latch */
	PIT_CONTROL  =	0x43,		/* read/write Chip's Control Word */
};

/*
 * Control Word format
 */
union pit_cmd {
	u8 raw;
	struct __attribute__ ((__packed__)){
		u8 bcd:1;		/* BCD format for counter divisor? */
		u8 mode:3;		/* Counter modes 0 to 5 */
		u8 rw:2;		/* Read/Write latch, LSB, MSB, or 16-bit */
		u8 timer:2;	/* Which timer of the three (0-2) */
	};
};

/*
 * Read/Write control bits
 */
enum {
	RW_LATCH =	0x0,		/* Counter latch command */
	RW_LSB   =	0x1,		/* Read/write least sig. byte only */
	RW_MSB   =	0x2,		/* Read/write most sig. byte only */
	RW_16bit =	0x3,		/* Read/write least then most sig byte */
};

/*
 * Counters modes
 */
enum {
	MODE_0 =	0x0,		/* Single interrupt on timeout */
	MODE_1 =	0x1,		/* Hardware retriggerable one shot */
	MODE_2 =	0x2,		/* Rate generator */
	MODE_3 =	0x3,		/* Square-wave mode */
};

/*
 * Start counter 2: raise the GATE-2 pin.
 * Disable glue between OUT-2 and the speaker in the process
 */
static inline void timer2_start(void)
{
	u8 val;

	val = (inb(0x61) | PIT_GATE2) & ~PIT_SPEAKER;
	outb(val, 0x61);
}

/*
 * Freeze counter 2: clear the GATE-2 pin.
 */
static inline void timer2_stop(void)
{
	u8 val;

	val = inb(0x61) & ~PIT_GATE2;
	outb(val, 0x61);
}

static void pit_set_counter(int ms, int counter_reg)
{
	u32 counter;
	u8 counter_low, counter_high;

	/* counter = ticks per second * seconds to delay
	 *         = PIT_CLOCK_RATE * (ms / 1000)
	 *         = PIT_CLOCK_RATE / (1000 / ms)
	 * We use last form to avoid float arithmetic */
	counter = PIT_CLOCK_RATE / (1000 / ms);

	counter_low = counter & 0xff;
	counter_high = counter >> 8;

	outb(counter_low, counter_reg);
	outb(counter_high, counter_reg);
}

void pit_mdelay(int ms)
{
	union pit_cmd cmd = { .raw = 0 };

	/* GATE-2 down */
	timer2_stop();

	cmd.bcd = 0;
	cmd.mode = MODE_0;
	cmd.rw = RW_16bit;
	cmd.timer = 2;
	outb(cmd.raw, PIT_CONTROL);

	pit_set_counter(ms, PIT_COUNTER2);

	/* GATE-2 up */
	timer2_start();

	while ((inb(0x61) & PIT_OUT2) == 0)
		cpu_pause();
}
void pit_5secs_delay(void)
{
	for (int i = 0; i < 500; i++)
		pit_mdelay(10);
}
