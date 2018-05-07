#include <inmate.h>

void __attribute__((noreturn)) c_entry(void);

void __attribute__((noreturn)) c_entry(void)
{
	inmate_main();
	stop();
}
