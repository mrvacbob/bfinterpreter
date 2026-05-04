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
#define NO_JNZ
#define NO_BENCHMARK
#include <err.h>
#include <sysexits.h>
#include "skeleton.h"

static void emit_c()
{
	instruction *inst = insts;
	value *val = vals;

	puts("#include <stdio.h>\n#include <inttypes.h>\n\n#define DEFAULT_TAPE_SIZE 4*1024*1024\n\ntypedef uint8_t cell;\n\nstatic cell tapebuf[DEFAULT_TAPE_SIZE] = {0};\n\nint main(int argc, char *argv[])\n{\n\tcell *tape = tapebuf;\n");

#define  def(n, v) case Op_##n: printf("\t" v ";\n"); break;
#define defb(n, v) case Op_##n: printf("\t" v  "\n"); val++; break;
#define defv(n, v) case Op_##n: printf("\t" v ";\n", val->off); val++; break;
#define defe(n, v) case Op_##n: printf("\t" v ";\n"); goto out; break;

	while (1) {
		switch (*inst++) {
			defv(PtrRight, "tape += %u")
			def(PtrRight1, "tape++")
			defv(PtrLeft, "tape -= %u")
			def(PtrLeft1, "tape--")
			defv(Inc, "(*tape) += %u")
			def(Inc1, "(*tape)++")
			defv(Dec, "(*tape) -= %u")
			def(Dec1, "(*tape)--")
			def(Putchar, "putchar_unlocked(*tape)")
			def(Getchar, "*tape = getchar_unlocked()")
			defb(Jz, "while (*tape) {")
			defb(Jmp, "}")
			def(Set0, "(*tape) = 0")
			def(Set1, "(*tape) = 1")
			defv(Set, "(*tape) = %u")
			defe(Return, "return 0")
			default: ;
		}
	}

out:
	puts("}");
}

int main(int argc, char *argv[])
{
	FILE *src;

	if (argc < 2) errx(EX_USAGE, "usage: %s file.bf", argv[0]);

	src = fopen(argv[1], "r");
	if (!src) err(EX_NOINPUT, "%s", argv[1]);
	parse(src);
	fclose(src);

	emit_c();

	return 0;
}