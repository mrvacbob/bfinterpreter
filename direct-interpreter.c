/*
 * Copyright (c) Alex Strange
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#define INSTRUCTION_TYPE const void*

#include <err.h>
#include <sysexits.h>
#include "skeleton.h"

static instruction *insn_ptrs;

static instruction compile(insn_name i)
{
	return insn_ptrs[i];
}

#ifdef DEBUG_IR
static insn_name decompile(instruction i)
{
	int j;

	for (j = 0; j < NUM_INSTRUCTIONS; j++) if (insn_ptrs[j] == i) return j;

	return Op_Return;
}
#endif

//goto labels are local (duh)
//so you have to call the function to export them
static void execute(instruction **insn_ptr_hack)
{
	static const void *labels[] = {&&L_PtrRight, &&L_PtrRight1, &&L_PtrLeft, &&L_PtrLeft1, &&L_Inc,
		&&L_Inc1, &&L_Dec, &&L_Dec1, &&L_Putchar, &&L_Getchar, &&L_Jmp,
		&&L_Jnz, &&L_Jz, &&L_Set0, &&L_Set1, &&L_Set, &&L_Return};

	if (unlikely(insn_ptr_hack != NULL)) {*insn_ptr_hack = labels; return;}

	cell *tape = tapebuf, *tape_begin = tape, *tape_end = tape + DEFAULT_TAPE_SIZE;
	instruction *inst = insts;
	value *val = vals, *o = val;

#define check_right(n)	if (unlikely((size_t)(tape_end - tape) <= (n))) die("tape fell off the right side")
#define check_left(n)	if (unlikely((n) > (size_t)(tape - tape_begin))) die("tape fell off the left side")
#define value()			o = val++;	
#define jmp()			{inst = o->rec.ip; val = o->rec.op;}
#define next()			{COUNT_INSN(); goto *(*inst++);}
#define Inst(n, i)		L_##n: i; next();

	next();

#include "instructions.h"

out:
	;
}

int main(int argc, char *argv[])
{
	FILE *src;
	int64_t time;

	if (argc < 2) errx(EX_USAGE, "usage: %s file.bf", argv[0]);

	execute(&insn_ptrs);

	src = fopen(argv[1], "r");
	if (!src) err(EX_NOINPUT, "%s", argv[1]);
	parse(src);
	fclose(src);

	time = benchmark_start();
	execute(NULL);
	benchmark(time, insts_executed, "directly-interpreted execution", "ops");

	return 0;
}