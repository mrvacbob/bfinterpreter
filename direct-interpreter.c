#define INSTRUCTION_TYPE const void*

#include "skeleton.h"

static instruction *insn_ptrs;

static instruction compile(insn_name i)
{
	return insn_ptrs[i];
}

static insn_name decompile(instruction i)
{
	int j;

	for (j = 0; j < NUM_INSTRUCTIONS; j++) if (insn_ptrs[j] == i) return j;

	return Op_Return;
}

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

#define check_right()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the right side");}
#define check_left()	if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the left side");}
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
	time_t time;

	if (argc < 2) return 1;

	execute(&insn_ptrs);

	src = fopen(argv[1], "r");	
	parse(src);
	fclose(src);

	time = benchmark_start();
	execute(NULL);
	benchmark(time, insts_executed, "directly-interpreted execution", "ops");

	return 0;
}