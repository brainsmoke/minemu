
/* This file is part of minemu
 *
 * Copyright 2010-2011 Erik Bosman <erik@minemu.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <string.h>
#include <stddef.h>

#include "jit.h"
#include "jit_code.h"
#include "jit_fragment.h"
#include "scratch.h"
#include "runtime.h"
#include "error.h"
#include "syscalls.h"
#include "mm.h"
#include "debug.h"
#include "exec_ctx.h"

/* On the event of a signal we may need to finish the emulation of an instruction.
 * So we need to translate our jit code of one emulated instruction into code which
 * switches back after that instruction is done.
 */

typedef long (*exit_func_t)(void);
const exit_func_t jit_fragment_exit_addr = jit_fragment_exit;

static long jit_fragment_jcc(char *dest, instr_t *instr, char *jump_jit_addr)
{
	return gen_code(
		dest,

		"0F 80+ L",                             /* jcc rel32                        */

		instr->addr[instr->mrm-1] & 0x0f,
		jump_jit_addr-&dest[6]
	);
}

static long jit_fragment_jcc_exit(char *dest, instr_t *instr, char *jump_addr)
{
	int len = gen_code(
		dest,

		"70+ 09"                                /* jcc end (inversed)               */
		"64 C7 05 L L",                         /* movl $addr, jit_eip              */

		(instr->addr[instr->mrm-1]^1) & 0x0f,
		offsetof(exec_ctx_t, jit_eip), jump_addr
	);
	return len+jump_to(&dest[len], (char *)(long)jit_fragment_exit);
}

static long jit_fragment_jump(char *dest, char *jump_jit_addr)
{
	return jump_to(dest, jump_jit_addr);
}

static long jit_fragment_jump_exit(char *dest, char *jump_addr)
{
	int len = gen_code(
		dest,

		"64 C7 05 L L",                            /* movl $addr, jit_eip               */

		offsetof(exec_ctx_t, jit_eip), jump_addr
	);
	return len+jump_to(&dest[len], (char *)(long)jit_fragment_exit);
}

static long jit_fragment_control(char *dest, instr_t *instr,
                                 char *addr, unsigned long len,
                                 char *mapping[])
{
	long code_len, off=0;
	char *pc = &instr->addr[instr->len],
	     *jump_addr = pc + imm_at(&instr->addr[instr->imm], instr->len-instr->imm);

	switch (jit_action[instr->op])
	{
		case JUMP_CONDITIONAL:
			if ( contains(addr, len+1, jump_addr) )
				return jit_fragment_jcc(dest, instr, mapping[jump_addr-addr]);
			else
				return jit_fragment_jcc_exit(dest, instr, jump_addr);

		case LOOP: /* loops */
			off = gen_code(
				dest,

				"$  02"
				"EB FF", /* jump filled in at 1.) */

				instr->addr, instr->len-1
			);

			if ( contains(addr, len+1, jump_addr) )
				code_len = off + jit_fragment_jump(&dest[off], mapping[jump_addr-addr]);
			else
				code_len = off + jit_fragment_jump_exit(&dest[off], jump_addr);

			dest[off-1] = code_len-off; /* 1.) */

			return code_len;

		case JUMP_RELATIVE:
			if ( contains(addr, len+1, jump_addr) )
				return jit_fragment_jump(dest, mapping[jump_addr-addr]);
			else if ( between(runtime_cache_resolution_start, runtime_cache_resolution_end, jump_addr) )
				return jit_fragment_jump(dest, jump_addr + (long)reloc_runtime_cache_resolution_start-
				                                                 (long)runtime_cache_resolution_start);
			else if ( contains(runtime_code_start, RUNTIME_CODE_SIZE, jump_addr) )
				return jit_fragment_jump(dest, jump_addr);
			else
				return jit_fragment_jump_exit(dest, jump_addr);

		case CALL_RELATIVE: /* unimplemented, not needed, (not used by code generation) */
		case CALL_INDIRECT:
		case JUMP_INDIRECT:
		case JUMP_FAR:
		case JOIN:
		case RETURN:
		case RETURN_CLEANUP:
		default:
			break;
	}
	die("unexpected translated code: %d", jit_action[instr->op]);
	return -1;
}

static char *jit_fragment_translate(char *addr, long len, char *entry,
                                    char jit_addr[], long jit_len, char *mapping[])
{
	instr_t instr;

	long s_off=0, d_off=0;
	char *jit_entry = NULL;

	for ( ;; )
	{
		mapping[s_off] = &jit_addr[d_off];

		if ( s_off > len )
			die("instuction sticks out of fragment");

		if ( s_off == len )
		{
			jit_fragment_jump_exit(&jit_addr[d_off], &addr[s_off]);
			break;
		}

		if ( &addr[s_off] == entry )
			jit_entry = &jit_addr[d_off];

		if ( read_op(&addr[s_off], &instr, len-s_off) == CUTOFF_OP )
			die("jit_fragment too large");

		if ( (jit_action[instr.op] & CONTROL_MASK) == CONTROL )
			d_off += jit_fragment_control(&jit_addr[d_off], &instr, addr, len, mapping);
		else
		{
			memcpy(&jit_addr[d_off], instr.addr, instr.len);
			d_off += instr.len;
		}

		s_off += instr.len;
	}

	if ( jit_entry == NULL )
		die("fragment entry point not in translated code");

	return jit_entry;
}

static char *jit_fragment(char *fragment, long len, char *entry)
{
	char *jit_entry;
	char *mapping[len+1];
	char *jit_fragment_page = get_exec_ctx()->jit_fragment_page;
	long code_sz = sizeof( ctx[0].jit_fragment_page );

	/* two-pass, we build up the jump-mapping beforehand */
	            jit_fragment_translate(fragment, len, entry, jit_fragment_page, code_sz, mapping);
	jit_entry = jit_fragment_translate(fragment, len, entry, jit_fragment_page, code_sz, mapping);

	return jit_entry;
}

char *jit_fragment_run(struct sigcontext *context);

/* INTERCAL's COME FROM is for wimps :-)
 *
 */
void finish_instruction(struct sigcontext *context)
{
	exec_ctx_t *local_ctx = get_exec_ctx();
	char *orig_eip, *jit_op_start;
	long jit_op_len;

	sys_mprotect(local_ctx->jit_fragment_page, PG_SIZE, PROT_READ|PROT_WRITE);

	if ( (orig_eip = jit_rev_lookup_addr((char *)context->eip, &jit_op_start, &jit_op_len)) )
	{
		if ( (char *)context->eip == jit_op_start ) /* we don't have to do anything */
		{
			context->eip = (long)orig_eip; /* set return instruction pointer to user eip */
			return;
		}
		/* jit the jit! */
		context->eip = (long)jit_fragment(jit_op_start, jit_op_len, (char *)context->eip);
	}
	else if ( between(syscall_intr_critical_start, syscall_intr_critical_end, (char *)context->eip) )
	{
		context->eip = (long)syscall_intr_critical_start;
	}
	else if ( between(runtime_cache_resolution_start, runtime_cache_resolution_end, (char *)context->eip) )
	{
		context->eip += reloc_runtime_cache_resolution_start-runtime_cache_resolution_start;
	}

	local_ctx->runtime_ijmp_addr = reloc_runtime_ijmp;
	local_ctx->jit_return_addr = reloc_jit_return;

	sys_mprotect(local_ctx->jit_fragment_page, PG_SIZE, PROT_EXEC|PROT_READ);
	jit_fragment_run(context);
	sys_mprotect(local_ctx->jit_fragment_page, PG_SIZE, PROT_READ|PROT_WRITE);

	local_ctx->runtime_ijmp_addr = runtime_ijmp;
	local_ctx->jit_return_addr = jit_return;

	sys_mprotect(local_ctx->jit_fragment_page, PG_SIZE, PROT_EXEC|PROT_READ);

	orig_eip = jit_rev_lookup_addr((char *)context->eip, &jit_op_start, &jit_op_len);
	if ( (char *)context->eip != jit_op_start )
		die("instruction pointer (%x) not at opcode start after jit_fragment_run()", context->eip);

	context->eip = (long)orig_eip;

	if (local_ctx->jit_fragment_restartsys)
		context->eip -= 2;
}


