#include "skeleton.h"

static void execute()
{
	static const void *labels[] = {&&L_PtrRight, &&L_PtrRight1, &&L_PtrLeft, &&L_PtrLeft1, &&L_Inc,
		&&L_Inc1, &&L_Dec, &&L_Dec1, &&L_Putchar, &&L_Getchar, &&L_Jmp,
		&&L_Jnz, &&L_Jz, &&L_Set0, &&L_Set1, &&L_Set, &&L_Return};

	cell *tape = tapebuf, *tape_begin = tape, *tape_end = tape + DEFAULT_TAPE_SIZE;
	instruction *inst = insts;
	value *val = vals, *o = val;

#define check_right()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the right side");}
#define check_left()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the left side");}
#define value()			o = val++;	
#define jmp()			{inst = o->rec.ip; val = o->rec.op;}
#define next()			{COUNT_INSN(); goto *labels[*inst++];}
#define Inst(n, i)		L_##n: i; next();

	next();

#include "instructions.h"

out:
	;
}

int main(int argc, char *argv[])
{
	FILE *src;
	time_t time;

	if (argc < 2) return 1;

	src = fopen(argv[1], "r");	
	parse(src);
	fclose(src);

	time = benchmark_start();
	execute();
	benchmark(time, insts_executed, "indirectly-interpreted execution", "ops");

	return 0;
}