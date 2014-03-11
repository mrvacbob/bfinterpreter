#include "skeleton.h"

void execute()
{
	cell *tape = tapebuf, *tape_begin = tape, *tape_end = tape + DEFAULT_TAPE_SIZE;
    instruction *inst = insts;
    value *val = vals, *o = val;
	
#define check_right() if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the right side");}
#define check_left() if (unlikely(tape < tape_begin || tape >= tape_end)) {die("tape fell off the left side");}
#define value() o = val++;	
#define jmp() {inst = o->rec.ip; val = o->rec.op;}
#define Inst(n, i) case Op_##n: i; break;

	while (1) {
        COUNT_INSN();
		switch (*inst++) {
			#include "instructions.h"
			default: ;
		}
	}
    
out: ;
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
    benchmark(time, insts_executed, "switched-interpreted execution", "ops");
	
	return 0;
}