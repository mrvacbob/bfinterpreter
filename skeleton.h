#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>

#define DEFAULT_TAPE_SIZE 4*1024*1024
#define MAX_INSTRUCTIONS 262144
#define MAX_LOOP_DEPTH 256

#ifdef __GNUC__
#define noreturn __attribute__((noreturn))
#define cold __attribute__((cold))
#define unlikely(x) __builtin_expect((x), 0)
#else
#define noreturn
#define cold
#define unlikely(x) (x)
#endif

typedef uint8_t cell;

/*
 operand values:
 for ptr/cell operations: one int
 for jumps: one jump_rec 
 */

typedef enum insn_name {
	Op_PtrRight,
	Op_PtrRight1,
	Op_PtrLeft,
	Op_PtrLeft1,
	Op_Inc,
	Op_Inc1,
	Op_Dec,
	Op_Dec1,
	Op_Putchar,
	Op_Getchar,
	Op_Jmp,
	Op_Jnz,
	Op_Jz,
	Op_Set0,
	Op_Set1,
	Op_Set,
	Op_Return,
	NUM_INSTRUCTIONS,
	No_Op,
	Other_Op
} insn_name;

#ifdef INSTRUCTION_TYPE
typedef INSTRUCTION_TYPE instruction;
static instruction compile(insn_name i);
static insn_name decompile(instruction i);
#else
typedef insn_name instruction;
static instruction compile(insn_name i) {return i;}
static insn_name decompile(instruction i) {return i;}
#endif

typedef struct jump_rec {
	instruction *ip;
	union value *op;
} jump_rec;

typedef union value {
	jump_rec rec;
	unsigned off;
} value;

static const char * const insn_strs[] = {
	"PtrRight", "PtrRight1", "PtrLeft", "PtrLeft1",
	"Inc", "Inc1", "Dec", "Dec1", "Putchar", "Getchar",
	"Jmp", "Jnz", "Jz", "Set0", "Set1", "Set", "Return"
};

static const enum value_types {
	Oper_None,
	Oper_Ptr,
	Oper_Cell,
	Oper_Jump
} inst_types[] = {
	Oper_Ptr, Oper_None, Oper_Ptr, Oper_None,
	Oper_Cell, Oper_None, Oper_Cell, Oper_None,
	Oper_None, Oper_None, Oper_Jump, Oper_Jump,
	Oper_Jump, Oper_None, Oper_None, Oper_Cell, Oper_None
};

static cell tapebuf[DEFAULT_TAPE_SIZE] = {0};
static instruction insts[MAX_INSTRUCTIONS];
static value vals[MAX_INSTRUCTIONS];

static unsigned insts_executed = 0;
#define COUNT_INSN() insts_executed++

static void *zalloc(int size)
{
	return calloc(size, 1);
}

static void noreturn die(const char *err)
{
	fprintf(stderr, "fatal error: %s\n", err);
	abort();
}

static void print_ir()
{
	insn_name op;
	int inst_i = 0, val_i = 0;
    instruction *inst = insts;
    value *val = vals;
	
	do {
		op = decompile(inst[inst_i]);
		printf(" %d | %d | ", inst_i, val_i);
		printf("%s\t", insn_strs[op]);
		
		switch (inst_types[op]) {
			case Oper_None: printf("\n"); break;
			case Oper_Ptr:  
			case Oper_Cell: printf("%u\n", val[val_i++].off); break;
			case Oper_Jump: {jump_rec *r = &val[val_i++].rec; printf("(%d, %d, =%s)\n", (int)(r->ip - insts), (int)(r->op - vals), insn_strs[decompile(*r->ip)]);} break;
		}
		
		inst_i++;
	} while (op != Op_Return);
}

static int is_op(int c)
{
	switch (c) {
		case '<':
		case '>':
		case '-':
		case '+':
		case '.':
		case ',':
		case '[':
		case ']': return 1;
		case EOF: return EOF;
		default:  return 0;
	}
}

static size_t parse_repeat(FILE *src, int first_i, char neg_c, char pos_c, insn_name neg_i, insn_name neg1_i, insn_name pos_i, insn_name pos1_i, insn_name *inst, int *next_i)
{
	int c;
	ssize_t start = (first_i==neg_c)?-1:1;
	
	while (1) {
		c = fgetc(src);
		
		if (c == neg_c) start--;
		else if (c == pos_c) start++;
		else if (is_op(c) != 0) break;
	}
	*next_i = c;
	
	if (!start) return 0;
	else if (abs(start) == 1) {
		*inst = (start == -1) ? neg1_i : pos1_i;
		return 0;
	}
	
	*inst = (start < 0) ? neg_i : pos_i;
	return abs(start);
}

static void parse(FILE *src)
{
	int c;
	int inst_i = 0, val_i = 0, loop_i = -1;
	size_t val;
	insn_name tmp_inst = Op_Return;
	value o;
	jump_rec r;
	jump_rec loops[MAX_LOOP_DEPTH];
	
#define pushinst(name)	{insts[inst_i++] = compile(name); if (inst_i >= MAX_INSTRUCTIONS) die("program too long");}
#define pushval() 		{vals[val_i++] = o; if (val_i >= MAX_INSTRUCTIONS) die("program too long");}
#define pushloop()		{if (loop_i >= MAX_LOOP_DEPTH) die("too many loops"); loops[++loop_i] = r;}
#define cur_pc()		(jump_rec){&insts[inst_i], &vals[val_i]}
	
	while (1) {
		c = fgetc(src);
	reparse:
		if (c == EOF) break;
		switch (c) {
			case '<': case '>':
				val = parse_repeat(src, c, '<', '>', Op_PtrLeft, Op_PtrLeft1, Op_PtrRight, Op_PtrRight1, &tmp_inst, &c);
				pushinst(tmp_inst);
				if (val) {o.off = val; pushval();}
				goto reparse;				
				break;
			case '-': case '+':
				val = parse_repeat(src, c, '-', '+', Op_Dec, Op_Dec1, Op_Inc, Op_Inc1, &tmp_inst, &c);
				if (insts[inst_i-1] == compile(Op_Set0)) {
					inst_i--;
					if (tmp_inst == Op_Inc1) {pushinst(Op_Set1);}
					else if (tmp_inst == Op_Inc) {pushinst(Op_Set);}
					else if (tmp_inst == Op_Dec) {pushinst(Op_Set); val = -val;}
				} else {pushinst(tmp_inst);}
				if (val) {o.off = val; pushval();}
				goto reparse;
				break;
			case '.':
				pushinst(Op_Putchar);
				break;
			case ',':
				pushinst(Op_Getchar);
				break;
			case '[':
				r = cur_pc();
				pushloop();
				pushinst(Op_Jz);
				pushval();
				break;
			case ']':
				o.rec = loops[loop_i--];

				if (insts[inst_i-1] == compile(Op_Dec1) && insts[inst_i-2] == compile(Op_Jz)) {
					inst_i -= 2;
					val_i--;
					pushinst(Op_Set0);
				} else {
#ifndef NO_JNZ
					//jmp to x = jnz to x+1
					pushinst(Op_Jnz);
					pushval();
					vals[val_i-1].rec.ip++;
					vals[val_i-1].rec.op++;
#else
					pushinst(Op_Jmp);
					pushval();
#endif
					o.rec.op->rec = cur_pc();
				}
				break;
		}
	}
	
	pushinst(Op_Return);
	
	if (loop_i > -1) die("unclosed loop");
	
#ifndef NO_PRINT_IR
	print_ir();
#endif
}

static time_t timeval_to_us(struct timeval *tv)
{
    return tv->tv_sec * 1000000 + tv->tv_usec;
}

static time_t benchmark(time_t last_time, unsigned task_count, const char *label, const char *task_name_plural)
{
    struct rusage rusage;
    time_t time;
    
    getrusage(RUSAGE_SELF, &rusage);
    
    time = timeval_to_us(&rusage.ru_utime) + timeval_to_us(&rusage.ru_stime);
    
    if (last_time) {
        float seconds = (time - last_time) / 1000000.;
        system("clear"); // :shobon: x10
        printf("\n%s took %fs (%f %s/sec)\n", label, seconds, task_count / seconds, task_name_plural);
    }
    
    return time;
}

static time_t benchmark_start()
{
    return benchmark(0, 1, "", ""); // :shobon:
}