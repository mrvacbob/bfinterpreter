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

/*
 * Code-copying JIT Brainfuck interpreter — x86-64 and arm64, macOS / Linux.
 *
 * Every machine-code template lives in the .text segment via global __asm__,
 * written as real assembly.  The JIT copies template bytes verbatim into a
 * MAP_JIT buffer; operands and branch displacements are patched in using
 * label-pointer arithmetic — no manually-typed encoding tables in C.
 *
 * Templates use three callee-saved registers, loaded in the JIT prologue:
 *   x86-64: r12=tape  r13=putchar_unlocked  r14=getchar_unlocked
 *   arm64:  x19=tape  x20=putchar_unlocked  x21=getchar_unlocked
 */

#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <err.h>
#include <sysexits.h>
#ifdef __APPLE__
#  include <pthread.h>
#  ifndef MAP_JIT
#    define MAP_JIT 0x800
#  endif
#  define JIT_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT)
#else
#  define JIT_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS)
#endif

#include "skeleton.h"

#ifdef __APPLE__
#  define _S(x) "_" x
#else
#  define _S(x) x
#endif

/* =========================================================================
 * Machine-code templates in .text
 *
 * Naming convention for patch labels:
 *   T_Foo_end    — one past the last byte of template Foo
 *   T_Foo_patch  — the byte/instruction that needs a runtime value written in
 *                  (for imm8/imm32: points to the immediate field;
 *                   for arm64 B:    points to the B instruction itself;
 *                   for arm64 MOVZ/MOVK sequences: see address patch helpers)
 * ========================================================================= */

#if defined(__x86_64__)

__asm__(
    ".text\n"

    /* --- simple ops: copy verbatim --- */
    _S("T_Inc1")       ": incb    (%r12)\n"                         _S("T_Inc1_end")       ":\n"
    _S("T_Dec1")       ": decb    (%r12)\n"                         _S("T_Dec1_end")       ":\n"
    _S("T_Set0")       ": movb    $0,(%r12)\n"                      _S("T_Set0_end")       ":\n"
    _S("T_Set1")       ": movb    $1,(%r12)\n"                      _S("T_Set1_end")       ":\n"
    _S("T_PtrRight1")  ": incq    %r12\n"                           _S("T_PtrRight1_end")  ":\n"
    _S("T_PtrLeft1")   ": decq    %r12\n"                           _S("T_PtrLeft1_end")   ":\n"
    _S("T_Putchar")    ": movzbl  (%r12),%edi\n  callq *%r13\n"    _S("T_Putchar_end")    ":\n"
    _S("T_Getchar")    ": callq   *%r14\n  movb %al,(%r12)\n"      _S("T_Getchar_end")    ":\n"

    /* --- operand ops: copy then patch immediate --- */
    /* imm8 ops: patch label is one past the imm8 byte */
    _S("T_Inc")        ": addb    $1,(%r12)\n"   _S("T_Inc_patch")      ":\n" _S("T_Inc_end")      ":\n"
    _S("T_Dec")        ": subb    $1,(%r12)\n"   _S("T_Dec_patch")      ":\n" _S("T_Dec_end")      ":\n"
    _S("T_Set")        ": movb    $1,(%r12)\n"   _S("T_Set_patch")      ":\n" _S("T_Set_end")      ":\n"
    /* imm32 ops: placeholder > 127 forces 32-bit encoding; patch label one past imm32 */
    _S("T_PtrRight")   ": addq    $0x101,%r12\n" _S("T_PtrRight_patch") ":\n" _S("T_PtrRight_end") ":\n"
    _S("T_PtrLeft")    ": subq    $0x101,%r12\n" _S("T_PtrLeft_patch")  ":\n" _S("T_PtrLeft_end")  ":\n"

    /* --- branch ops: patch label is the rel32 field --- */
    _S("T_Jz")   ": cmpb $0,(%r12)\n  .byte 0x0f,0x84\n"  _S("T_Jz_patch")  ": .long 0\n" _S("T_Jz_end")  ":\n"
    _S("T_Jnz")  ": cmpb $0,(%r12)\n  .byte 0x0f,0x85\n"  _S("T_Jnz_patch") ": .long 0\n" _S("T_Jnz_end") ":\n"
    _S("T_Jmp")  ": .byte 0xe9\n"                          _S("T_Jmp_patch") ": .long 0\n" _S("T_Jmp_end") ":\n"

    /* --- prologue: save r12/r13/r14, load tape/putchar/getchar ---
     * patch labels are one past each movabsq's 8-byte immediate */
    _S("T_Prologue")
    ":  pushq   %r12\n"
    "   pushq   %r13\n"
    "   pushq   %r14\n"
    "   movabsq $0xCCCCCCCCCCCCCCCC,%r12\n" _S("T_Prologue_tape") ":\n"
    "   movabsq $0xCCCCCCCCCCCCCCCC,%r13\n" _S("T_Prologue_putc") ":\n"
    "   movabsq $0xCCCCCCCCCCCCCCCC,%r14\n" _S("T_Prologue_getc") ":\n"
                                             _S("T_Prologue_end")  ":\n"

    /* --- epilogue: restore r14/r13/r12, ret --- */
    _S("T_Epilogue")
    ":  popq    %r14\n"
    "   popq    %r13\n"
    "   popq    %r12\n"
    "   retq\n"                              _S("T_Epilogue_end")  ":\n"
);

extern char T_Inc1[], T_Inc1_end[];
extern char T_Dec1[], T_Dec1_end[];
extern char T_Set0[], T_Set0_end[];
extern char T_Set1[], T_Set1_end[];
extern char T_PtrRight1[], T_PtrRight1_end[];
extern char T_PtrLeft1[],  T_PtrLeft1_end[];
extern char T_Putchar[],   T_Putchar_end[];
extern char T_Getchar[],   T_Getchar_end[];
extern char T_Inc[],       T_Inc_patch[],       T_Inc_end[];
extern char T_Dec[],       T_Dec_patch[],       T_Dec_end[];
extern char T_Set[],       T_Set_patch[],       T_Set_end[];
extern char T_PtrRight[],  T_PtrRight_patch[],  T_PtrRight_end[];
extern char T_PtrLeft[],   T_PtrLeft_patch[],   T_PtrLeft_end[];
extern char T_Jz[],        T_Jz_patch[],        T_Jz_end[];
extern char T_Jnz[],       T_Jnz_patch[],       T_Jnz_end[];
extern char T_Jmp[],       T_Jmp_patch[],       T_Jmp_end[];
extern char T_Prologue[],  T_Prologue_tape[], T_Prologue_putc[], T_Prologue_getc[], T_Prologue_end[];
extern char T_Epilogue[],  T_Epilogue_end[];

static inline void copy_tmpl(uint8_t **p, char *s, char *e)
{ size_t n=(size_t)(e-s); memcpy(*p,s,n); *p+=n; }

/* Patch an imm8 field: label sits one past the byte to patch. */
static inline void patch_imm8(uint8_t *base, char *lbl, char *start, uint8_t v)
{ base[(size_t)(lbl-start)-1] = v; }

/* Patch an imm32 field: label sits one past the 4-byte field to patch. */
static inline void patch_imm32(uint8_t *base, char *lbl, char *start, uint32_t v)
{ memcpy(base+(size_t)(lbl-start)-4, &v, 4); }

/* Patch a rel32 displacement field (je/jne/jmp). */
static inline void patch_rel32(uint8_t *field, uint8_t *target)
{ int32_t rel=(int32_t)(target-(field+4)); memcpy(field,&rel,4); }

/* Patch a 64-bit address into a movabsq: label sits one past the 8-byte imm. */
static inline void patch_addr64(uint8_t *base, char *lbl, char *start, uint64_t v)
{ memcpy(base+(size_t)(lbl-start)-8, &v, 8); }

static void emit_prologue(uint8_t **p)
{
    uint8_t *s = *p;
    copy_tmpl(p, T_Prologue, T_Prologue_end);
    patch_addr64(s, T_Prologue_tape, T_Prologue, (uint64_t)(uintptr_t)tapebuf);
    patch_addr64(s, T_Prologue_putc, T_Prologue, (uint64_t)(uintptr_t)putchar_unlocked);
    patch_addr64(s, T_Prologue_getc, T_Prologue, (uint64_t)(uintptr_t)getchar_unlocked);
}
static void emit_epilogue(uint8_t **p) { copy_tmpl(p, T_Epilogue, T_Epilogue_end); }

static void emit_inc(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Inc,T_Inc_end); patch_imm8(s,T_Inc_patch,T_Inc,n); }

static void emit_dec(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Dec,T_Dec_end); patch_imm8(s,T_Dec_patch,T_Dec,n); }

static void emit_set(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Set,T_Set_end); patch_imm8(s,T_Set_patch,T_Set,n); }

static void emit_ptr_right(uint8_t **p, uint32_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_PtrRight,T_PtrRight_end); patch_imm32(s,T_PtrRight_patch,T_PtrRight,n); }

static void emit_ptr_left(uint8_t **p, uint32_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_PtrLeft,T_PtrLeft_end); patch_imm32(s,T_PtrLeft_patch,T_PtrLeft,n); }

static uint8_t *emit_jz(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jz,T_Jz_end); return s+(T_Jz_patch-T_Jz); }

static uint8_t *emit_jnz(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jnz,T_Jnz_end); return s+(T_Jnz_patch-T_Jnz); }

static uint8_t *emit_jmp(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jmp,T_Jmp_end); return s+(T_Jmp_patch-T_Jmp); }

/* patch_jcc: writes a rel32 displacement into the field returned by emit_j*. */
static void patch_jcc(uint8_t *field, uint8_t *target) { patch_rel32(field, target); }

#elif defined(__aarch64__)

/*
 * arm64 templates.  x19=tape, x20=putchar, x21=getchar (all callee-saved).
 * ldrb/strb zero-extend; 32-bit add/sub wraps correctly when result stored
 * via strb (natural uint8_t semantics).
 */
__asm__(
    ".text\n"

    /* --- simple ops --- */
    _S("T_Inc1")      ": ldrb w0,[x19]\n  add w0,w0,#1\n  strb w0,[x19]\n" _S("T_Inc1_end")      ":\n"
    _S("T_Dec1")      ": ldrb w0,[x19]\n  sub w0,w0,#1\n  strb w0,[x19]\n" _S("T_Dec1_end")      ":\n"
    _S("T_Set0")      ": strb  wzr,[x19]\n"                                  _S("T_Set0_end")      ":\n"
    _S("T_Set1")      ": mov   w0,#1\n  strb w0,[x19]\n"                    _S("T_Set1_end")      ":\n"
    _S("T_PtrRight1") ": add   x19,x19,#1\n"                                _S("T_PtrRight1_end") ":\n"
    _S("T_PtrLeft1")  ": sub   x19,x19,#1\n"                                _S("T_PtrLeft1_end")  ":\n"
    _S("T_Putchar")   ": ldrb  w0,[x19]\n  blr x20\n"                       _S("T_Putchar_end")   ":\n"
    _S("T_Getchar")   ": blr   x21\n  strb w0,[x19]\n"                      _S("T_Getchar_end")   ":\n"

    /* --- operand ops ---
     * patch label sits one past the instruction whose immediate needs patching */
    /* Inc: ldrb; add w0,w0,#N (imm12); strb — patch label after the add */
    _S("T_Inc")   ": ldrb w0,[x19]\n  add w0,w0,#255\n" _S("T_Inc_patch") ":\n  strb w0,[x19]\n" _S("T_Inc_end") ":\n"
    _S("T_Dec")   ": ldrb w0,[x19]\n  sub w0,w0,#255\n" _S("T_Dec_patch") ":\n  strb w0,[x19]\n" _S("T_Dec_end") ":\n"
    /* Set: movz w0,#N (imm16); strb — patch label after movz */
    _S("T_Set")   ": movz w0,#255\n"                    _S("T_Set_patch") ":\n  strb w0,[x19]\n" _S("T_Set_end") ":\n"
    /* PtrRight/Left: movz x0,#lo; movk x0,#hi,lsl#16; add/sub x19,x19,x0
     * Two patch labels: _lo after movz, _hi after movk */
    _S("T_PtrRight") ": movz x0,#0xCCCC\n" _S("T_PtrRight_lo") ":\n"
                       "movk x0,#0xCCCC,lsl #16\n" _S("T_PtrRight_hi") ":\n"
                       "add  x19,x19,x0\n"          _S("T_PtrRight_end") ":\n"
    _S("T_PtrLeft")  ": movz x0,#0xCCCC\n" _S("T_PtrLeft_lo") ":\n"
                       "movk x0,#0xCCCC,lsl #16\n" _S("T_PtrLeft_hi") ":\n"
                       "sub  x19,x19,x0\n"           _S("T_PtrLeft_end") ":\n"

    /* --- branch ops ---
     * Use ldrb + inverted CBZ/CBNZ to skip a patchable unconditional B.
     * B has imm26 range ±128 MB, enough for a 64 MB JIT buffer.
     * patch label sits AT the B instruction. */
    _S("T_Jz")
    ":  ldrb  w0,[x19]\n"
    "   cbnz  w0,1f\n"        /* if tape!=0 skip the branch */
    _S("T_Jz_patch")  ":  b .\n"   /* placeholder; patched to forward target */
    "1:\n"                    _S("T_Jz_end")  ":\n"

    _S("T_Jnz")
    ":  ldrb  w0,[x19]\n"
    "   cbz   w0,1f\n"        /* if tape==0 skip the branch */
    _S("T_Jnz_patch") ":  b .\n"
    "1:\n"                    _S("T_Jnz_end") ":\n"

    _S("T_Jmp")  ":  b .\n"  _S("T_Jmp_patch") ":\n" _S("T_Jmp_end") ":\n"

    /* --- prologue: sub sp; stp/str; movz/movk×4 per register ---
     * patch labels sit one past each group of 4 movz/movk instructions */
    _S("T_Prologue")
    ":  sub  sp,sp,#32\n"
    "   stp  x19,x20,[sp,#0]\n"
    "   str  x21,[sp,#16]\n"
    "   movz x19,#0xCCCC\n  movk x19,#0xCCCC,lsl #16\n"
    "   movk x19,#0xCCCC,lsl #32\n  movk x19,#0xCCCC,lsl #48\n"
                                             _S("T_Prologue_tape") ":\n"
    "   movz x20,#0xCCCC\n  movk x20,#0xCCCC,lsl #16\n"
    "   movk x20,#0xCCCC,lsl #32\n  movk x20,#0xCCCC,lsl #48\n"
                                             _S("T_Prologue_putc") ":\n"
    "   movz x21,#0xCCCC\n  movk x21,#0xCCCC,lsl #16\n"
    "   movk x21,#0xCCCC,lsl #32\n  movk x21,#0xCCCC,lsl #48\n"
                                             _S("T_Prologue_getc") ":\n"
                                             _S("T_Prologue_end")  ":\n"

    /* --- epilogue --- */
    _S("T_Epilogue")
    ":  ldr  x21,[sp,#16]\n"
    "   ldp  x19,x20,[sp,#0]\n"
    "   add  sp,sp,#32\n"
    "   ret\n"                               _S("T_Epilogue_end")  ":\n"
);

extern char T_Inc1[], T_Inc1_end[];
extern char T_Dec1[], T_Dec1_end[];
extern char T_Set0[], T_Set0_end[];
extern char T_Set1[], T_Set1_end[];
extern char T_PtrRight1[], T_PtrRight1_end[];
extern char T_PtrLeft1[],  T_PtrLeft1_end[];
extern char T_Putchar[],   T_Putchar_end[];
extern char T_Getchar[],   T_Getchar_end[];
extern char T_Inc[],  T_Inc_patch[],  T_Inc_end[];
extern char T_Dec[],  T_Dec_patch[],  T_Dec_end[];
extern char T_Set[],  T_Set_patch[],  T_Set_end[];
extern char T_PtrRight[], T_PtrRight_lo[], T_PtrRight_hi[], T_PtrRight_end[];
extern char T_PtrLeft[],  T_PtrLeft_lo[],  T_PtrLeft_hi[],  T_PtrLeft_end[];
extern char T_Jz[],   T_Jz_patch[],   T_Jz_end[];
extern char T_Jnz[],  T_Jnz_patch[],  T_Jnz_end[];
extern char T_Jmp[],  T_Jmp_patch[],  T_Jmp_end[];
extern char T_Prologue[], T_Prologue_tape[], T_Prologue_putc[], T_Prologue_getc[], T_Prologue_end[];
extern char T_Epilogue[], T_Epilogue_end[];

static inline void copy_tmpl(uint8_t **p, char *s, char *e)
{ size_t n=(size_t)(e-s); memcpy(*p,s,n); *p+=n; }

/* Patch an arm64 imm12 field (bits [21:10]): label one past the instruction. */
static inline void patch_imm12(uint8_t *base, char *lbl, char *start, uint16_t v)
{
    uint32_t insn;
    memcpy(&insn, base+(size_t)(lbl-start)-4, 4);
    insn = (insn & ~(0xFFFu << 10)) | ((uint32_t)(v & 0xFFF) << 10);
    memcpy(base+(size_t)(lbl-start)-4, &insn, 4);
}

/* Patch an arm64 imm16 field (bits [20:5]): label one past the instruction. */
static inline void patch_imm16(uint8_t *base, char *lbl, char *start, uint16_t v)
{
    uint32_t insn;
    memcpy(&insn, base+(size_t)(lbl-start)-4, 4);
    insn = (insn & 0xFFE0001Fu) | ((uint32_t)v << 5);
    memcpy(base+(size_t)(lbl-start)-4, &insn, 4);
}

/* Patch all four movz/movk instructions for a 64-bit address.
 * lbl is one past the last of the four instructions. */
static void patch_addr64(uint8_t *base, char *lbl, char *start, uint64_t v)
{
    patch_imm16(base, lbl-12, start, (uint16_t)(v      ));  /* movz, chunk 0 */
    patch_imm16(base, lbl- 8, start, (uint16_t)(v >> 16));  /* movk lsl 16   */
    patch_imm16(base, lbl- 4, start, (uint16_t)(v >> 32));  /* movk lsl 32   */
    patch_imm16(base, lbl,    start, (uint16_t)(v >> 48));  /* movk lsl 48   */
}

static void emit_prologue(uint8_t **p)
{
    uint8_t *s = *p;
    copy_tmpl(p, T_Prologue, T_Prologue_end);
    patch_addr64(s, T_Prologue_tape, T_Prologue, (uint64_t)(uintptr_t)tapebuf);
    patch_addr64(s, T_Prologue_putc, T_Prologue, (uint64_t)(uintptr_t)putchar_unlocked);
    patch_addr64(s, T_Prologue_getc, T_Prologue, (uint64_t)(uintptr_t)getchar_unlocked);
}
static void emit_epilogue(uint8_t **p) { copy_tmpl(p, T_Epilogue, T_Epilogue_end); }

static void emit_inc(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Inc,T_Inc_end); patch_imm12(s,T_Inc_patch,T_Inc,n); }

static void emit_dec(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Dec,T_Dec_end); patch_imm12(s,T_Dec_patch,T_Dec,n); }

static void emit_set(uint8_t **p, uint8_t n)
{ uint8_t *s=*p; copy_tmpl(p,T_Set,T_Set_end); patch_imm16(s,T_Set_patch,T_Set,n); }

static void emit_ptr_right(uint8_t **p, uint32_t n)
{
    uint8_t *s=*p; copy_tmpl(p,T_PtrRight,T_PtrRight_end);
    patch_imm16(s, T_PtrRight_lo, T_PtrRight, (uint16_t)(n      ));
    patch_imm16(s, T_PtrRight_hi, T_PtrRight, (uint16_t)(n >> 16));
}

static void emit_ptr_left(uint8_t **p, uint32_t n)
{
    uint8_t *s=*p; copy_tmpl(p,T_PtrLeft,T_PtrLeft_end);
    patch_imm16(s, T_PtrLeft_lo, T_PtrLeft, (uint16_t)(n      ));
    patch_imm16(s, T_PtrLeft_hi, T_PtrLeft, (uint16_t)(n >> 16));
}

/* emit_j*: returns pointer to the B instruction to patch. */
static uint8_t *emit_jz(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jz,T_Jz_end); return s+(T_Jz_patch-T_Jz); }

static uint8_t *emit_jnz(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jnz,T_Jnz_end); return s+(T_Jnz_patch-T_Jnz); }

static uint8_t *emit_jmp(uint8_t **p)
{ uint8_t *s=*p; copy_tmpl(p,T_Jmp,T_Jmp_end); return s+(T_Jmp_patch-T_Jmp); }

/* patch_jcc: rewrites a B instruction to branch to target. */
static void patch_jcc(uint8_t *insn, uint8_t *target)
{
    uint32_t b;
    memcpy(&b, insn, 4);
    int32_t imm26 = (int32_t)((target - insn) / 4);
    b = (b & 0xFC000000u) | ((uint32_t)imm26 & 0x3FFFFFFu);
    memcpy(insn, &b, 4);
}

#else
#  error "Unsupported architecture: only x86-64 and arm64 are implemented"
#endif

/* =========================================================================
 * JIT compilation (ISA-independent IR walk)
 * ========================================================================= */

typedef struct { uint8_t *patch; int target_ir; } fixup_t;
static fixup_t fixups[MAX_INSTRUCTIONS];
static int     nfixups;
static size_t  ir_offsets[MAX_INSTRUCTIONS];

static void jit_compile(uint8_t *buf, size_t *used)
{
    uint8_t    *p    = buf;
    instruction *inst = insts;
    value       *val  = vals;

    nfixups = 0;
    emit_prologue(&p);

    for (int i = 0; ; i++) {
        insn_name op = *inst++;
        ir_offsets[i] = (size_t)(p - buf);

        switch (op) {
        case Op_Inc1:      copy_tmpl(&p, T_Inc1,      T_Inc1_end);      break;
        case Op_Dec1:      copy_tmpl(&p, T_Dec1,      T_Dec1_end);      break;
        case Op_Set0:      copy_tmpl(&p, T_Set0,      T_Set0_end);      break;
        case Op_Set1:      copy_tmpl(&p, T_Set1,      T_Set1_end);      break;
        case Op_PtrRight1: copy_tmpl(&p, T_PtrRight1, T_PtrRight1_end); break;
        case Op_PtrLeft1:  copy_tmpl(&p, T_PtrLeft1,  T_PtrLeft1_end);  break;
        case Op_Putchar:   copy_tmpl(&p, T_Putchar,   T_Putchar_end);   break;
        case Op_Getchar:   copy_tmpl(&p, T_Getchar,   T_Getchar_end);   break;
        case Op_Inc:       emit_inc(&p,       (uint8_t)val->off); val++; break;
        case Op_Dec:       emit_dec(&p,       (uint8_t)val->off); val++; break;
        case Op_PtrRight:  emit_ptr_right(&p, val->off);          val++; break;
        case Op_PtrLeft:   emit_ptr_left(&p,  val->off);          val++; break;
        case Op_Set:       emit_set(&p,       (uint8_t)val->off); val++; break;
        case Op_Jz: {
            int tgt = (int)(val->rec.ip - insts); val++;
            fixups[nfixups++] = (fixup_t){emit_jz(&p), tgt};
            break;
        }
        case Op_Jnz: {
            int tgt = (int)(val->rec.ip - insts); val++;
            patch_jcc(emit_jnz(&p), buf + ir_offsets[tgt]);
            break;
        }
        case Op_Jmp: {
            int tgt = (int)(val->rec.ip - insts); val++;
            patch_jcc(emit_jmp(&p), buf + ir_offsets[tgt]);
            break;
        }
        case Op_Return:
            emit_epilogue(&p);
            goto done;
        default:
            die("unknown opcode in JIT");
        }
        COUNT_INSN();
    }
done:
    for (int i = 0; i < nfixups; i++)
        patch_jcc(fixups[i].patch, buf + ir_offsets[fixups[i].target_ir]);
    *used = (size_t)(p - buf);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char *argv[])
{
    FILE    *src;
    int64_t  t;
    size_t   bufsz = 64 * 1024 * 1024;
    size_t   used  = 0;

    if (argc < 2) errx(EX_USAGE, "usage: %s file.bf", argv[0]);
    src = fopen(argv[1], "r");
    if (!src) err(EX_NOINPUT, "%s", argv[1]);
    parse(src);
    fclose(src);

    uint8_t *buf = mmap(NULL, bufsz, PROT_READ | PROT_WRITE | PROT_EXEC,
                        JIT_FLAGS, -1, 0);
    if (buf == MAP_FAILED) err(EX_OSERR, "mmap");

#if defined(__APPLE__) && defined(__aarch64__)
    pthread_jit_write_protect_np(0);
#endif

    insts_executed = 0;
    jit_compile(buf, &used);

#if defined(__APPLE__) && defined(__aarch64__)
    pthread_jit_write_protect_np(1);
    __builtin___clear_cache((char *)buf, (char *)(buf + used));
#endif

    t = benchmark_start();
    ((void (*)(void))buf)();
    benchmark(t, insts_executed, "code-copying jit execution", "ops");

    munmap(buf, bufsz);
    return 0;
}
