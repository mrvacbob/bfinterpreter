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
#include <err.h>
#include <sysexits.h>
#include "skeleton.h"

void execute()
{
	cell *tape = tapebuf, *tape_begin = tape, *tape_end = tape + DEFAULT_TAPE_SIZE;
	instruction *inst = insts;
	value *val = vals, *o = val;

#define check_right()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the right side");}
#define check_left()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the left side");}
#define value()			o = val++;	
#define jmp()			{inst = o->rec.ip; val = o->rec.op;}
#define Inst(n, i)		case Op_##n: i; break;

	while (1) {
		COUNT_INSN();
		switch (*inst++) {
			#include "instructions.h"
			default: ;
		}
	}

out:
	;
}

int main(int argc, char *argv[])
{
	FILE *src;
	time_t time;

	if (argc < 2) errx(EX_USAGE, "usage: %s file.bf", argv[0]);

	src = fopen(argv[1], "r");
	if (!src) err(EX_NOINPUT, "%s", argv[1]);
	parse(src);
	fclose(src);

	time = benchmark_start();
	execute();
	benchmark(time, insts_executed, "switched-interpreted execution", "ops");

	return 0;
}