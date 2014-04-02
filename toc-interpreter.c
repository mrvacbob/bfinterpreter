#define NO_JNZ
#define NO_PRINT_IR
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
			def(Putchar, "putchar(*tape)")
			def(Getchar, "*tape = getchar()")
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
	time_t time;

	if (argc < 2) return 1;

	src = fopen(argv[1], "r");
	parse(src);
	fclose(src);

	emit_c();

	return 0;
}