
#include <sys/mman.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "codeexec.h"
#include "opcodes.h"
#include "taint.h"

long taint_tmp[1];

long imm_at(char *addr, long size)
{
	long imm=0;
	memcpy(&imm, addr, size);
	if (size == 1)
		return *(signed char*)&imm;
	else
		return imm;
}

void imm_to(char *dest, long imm)
{
	memcpy(dest, &imm, sizeof(long));
}

int die()
{
	return -1;
}

unsigned long do_lea(unsigned char *m, int mlen)
{
	unsigned char opcode[mlen+1];

	opcode[0] = 0x8D;
	memcpy(&opcode[1], m, mlen);

	unsigned long regs[9] =
	{
		0x10000000,
		0x01000000,
		0x00100000,
		0x00010000,
		0x00001000,
		0x00000100,
		0x00000010,
		0x00000001,
		0x09000246, /* flags */
	};

	codeexec((char *)opcode, mlen+1, (long *)regs);
	return regs[0];
}

void test_mem_offset(unsigned char *m, int mlen)
{
	unsigned char m_off[16], m_off_len;
	unsigned long offset = 0x87654321;

	m_off_len = offset_mem((char *)m_off, (char *)m, offset);

	unsigned long addr     = do_lea(m, mlen),
	              addr_off = do_lea(m_off, m_off_len);

	if (offset != addr_off-addr)
	{
		printf("%08lx %08lx %08lx (", addr, addr_off, addr_off-addr);

		int i;
		for (i=0; i<mlen; i++)
			printf("%02X", m[i]);

		printf(") => (");

		for (i=0; i<m_off_len; i++)
			printf("%02X", m_off[i]);

		printf(")\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	unsigned char m[16];
	int mrm, sib;

	long disp32 = 0x80808080;
	char disp8  = 0x08;
	m[0] = 0x8D;

	for (mrm=0; mrm<0x100; mrm++)
	{
		if ( ( (mrm & 0xC0) == 0xC0 ) || /* RM is a register */
		     ( (mrm & 0x38) != 0x00 ) )  /* REG != %eax */
			continue;

		m[0] = mrm;

		if ( (mrm & 0x7) == 0x04 )
			for (sib=0; sib<0x100; sib++)
			{
				m[1] = sib;
				if ( (mrm & 0xC0) == 0x40 )
				{
					m[2] = disp8;
					test_mem_offset(m, 3);
				}
				else if ( ( (mrm & 0xC0) == 0x80 ) ||
				          ( (sib & 0x07) == 0x05 ) )
				{
					imm_to((char*)&m[2], disp32);
					test_mem_offset(m, 6);
				}
				else
					test_mem_offset(m, 2);
			}
		else
		{
			if ( (mrm & 0xC0) == 0x40 )
			{
				m[1] = disp8;
				test_mem_offset(m, 2);
			}
			else if ( ( (mrm & 0xC0) == 0x80 ) ||
			          ( (mrm & 0xC7) == 0x05 ) )
			{
				imm_to((char *)&m[1], disp32);
				test_mem_offset(m, 5);
			}
			else
				test_mem_offset(m, 1);

		}
	}
	exit(EXIT_SUCCESS);
}

