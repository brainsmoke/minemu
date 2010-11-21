
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "jit_fragment.h"
#include "syscalls.h"
#include "error.h"
#include "debug.h"
#include "hexdump.h"
#include "jit_code.h"
#include "taint.h"

char *jit_fragment_exit_eip;
char *jit_fragment_exit_addr;

char runtime_code_start[1];
long runtime_code_size = 0;

int fd_vprintf(int fd, const char *format, va_list ap)
{
	return vprintf(format, ap);
}

int fd_printf(int fd, const char *format, ...)
{
    int ret;
    va_list ap;
    va_start(ap, format);
    ret=fd_vprintf(fd, format, ap);
    va_end(ap);
    return ret;
}

/* make symbols resolve */
void runtime_ijmp(void) { die("calling placeholder"); }
void runtime_ret_cleanup(void) { die("calling placeholder"); }
void int80_emu(void) { die("calling placeholder"); }
void linux_sysenter_emu(void) { die("calling placeholder"); }
void jit_fragment_exit(void) { die("calling placeholder"); }
void jit_rev_lookup_addr(void) { die("calling placeholder"); }
char *hexcat(char *dest, unsigned long ul) { die("calling placeholder"); return NULL; }
int taint_flag = TAINT_ON;

char *tests[] =
{

/* relative jump */
	/* internal jump */
	" E9 00 00 00 00 90 90",
	" E9 00 00 00 00",
	" E9 01 00 00 00 90",
	" 90 90 E9 00 00 00 00 90 90",
	" 90&90 E9 00 00 00 00 90 90",
	" 90 90&E9 00 00 00 00 90 90",
	" 90 90 90 90 E9 00 00 00 00",
	" 90&90 90 90 E9 00 00 00 00",
	" 90 90&90 90 E9 00 00 00 00",
	" 90 90 90&90 E9 00 00 00 00",
	" 90 90 90 90&E9 00 00 00 00",
	" E9 FB FF FF FF",
	" 90 E9 FB FF FF FF",
	" 90&E9 FB FF FF FF",
	" 90&E9 FA FF FF FF",

	" EB 01 90 90",
	" EB 02 90 90",
	" EB 00",
	" 90 90 EB 00 90 90",
	" 90&90 EB 00 90 90",
	" 90 90&EB 00 90 90",
	" 90 90 90 90 EB 00",
	" 90&90 90 90 EB 00",
	" 90 90&90 90 EB 00",
	" 90 90 90&90 EB 00",
	" 90 90 90 90&EB 00",
	" EB FE",
	" 90 EB FE",
	" 90&EB FD",

	/* external jump */
	" E9 01 00 00 00",
	" E9 F1 FF FF FF",
	" E9 FA FF FF FF",
	" EB 01",
	" EB F1",
	" EB FA",

/* conditional relative */

	/* internal jump */
	"70 00",
	"70 FE",
	"90 70 FE",
	"90 70 FD",
	"90 70 01 90",

	"0F 80 00 00 00 00",
	"0F 80 FA FF FF FF",
	"90 0F 80 FA FF FF FF",
	"90 0F 80 F9 FF FF FF",
	"90 0F 80 01 00 00 00 90",

	/* external jump */
	"70 01",
	"70 FD",
	"90 70 FC",
	"90 70 02 90",

	"0F 80 01 00 00 00",
	"0F 80 F9 FF FF FF",
	"90 0F 80 F8 FF FF FF",
	"90 0F 80 02 00 00 00 90",

/* indirect jump/call */

	"FF 25 11 22 33 44",
	"FF 15 11 22 33 44",
	"ff 94 d8 44 33 22 11",
	"ff a4 d8 44 33 22 11",

/* loop{,n,nz} */

	/* internal jump */
	"E0 00",
	"E0 FE",
	"90 E0 FE",
	"90 E0 FD",
	"90 E0 01 90",

	"E1 00",
	"E1 FE",
	"90 E1 FE",
	"90 E1 FD",
	"90 E1 01 90",

	"E2 00",
	"E2 FE",
	"90 E2 FE",
	"90 E2 FD",
	"90 E2 01 90",

	/* external jump */
	"E0 01",
	"E0 FD",
	"90 E0 FC",
	"90 E0 02 90",

	"E1 01",
	"E1 FD",
	"90 E1 FC",
	"90 E1 02 90",

	"E2 01",
	"E2 FD",
	"90 E2 FC",
	"90 E2 02 90",

	NULL
};

extern char jit_fragment_page[];

long common_len(const char *s1, const char *s2)
{
	long i;
	for (i=0; s1[i] == s2[i]; i++);
	return i;
}

int main(void)
{
	char *entry;
	long i, entry_off, len, jit_len;
	char code[256];
	char diff_hack[4096];

	for (i=0; tests[i]; i++)
	{
		memset(code, 0, 256);
		entry_off = 0;
 		len = gen_code(code, tests[i], &entry_off);

		memset(jit_fragment_page, 0, 4096);
		entry = jit_fragment(code, len, &code[entry_off]);
		memcpy(diff_hack, jit_fragment_page, 4096);
		memset(jit_fragment_page, 1, 4096);
		entry = jit_fragment(code, len, &code[entry_off]);

		jit_len = common_len(diff_hack, jit_fragment_page);

		debug("entry %d -> %d, start/end %x/%x", entry_off, entry-jit_fragment_page, code, &code[len]);
		hexdump(2, code, len, 0, 0, NULL, NULL, NULL);
		hexdump(2, jit_fragment_page, jit_len, 0, 0, NULL, NULL, NULL);
		debug("");
	}
}
