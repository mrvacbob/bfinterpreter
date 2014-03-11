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
Inst(PtrRight, {value(); tape += o->off; check_right();})
Inst(PtrRight1, {tape++; check_right();})
Inst(PtrLeft, {value(); tape -= o->off; check_left();})
Inst(PtrLeft1, {tape--; check_left();})
Inst(Inc, {value(); *tape += o->off;})
Inst(Inc1, {(*tape)++;})
Inst(Dec, {value(); *tape -= o->off;})
Inst(Dec1, {(*tape)--;})
Inst(Putchar, {putchar_unlocked(*tape);})
Inst(Getchar, {*tape = getchar_unlocked();})
Inst(Jmp, {value(); jmp();})
//Jnz must only be generated if it would be equivalent to Jmp
Inst(Jnz, {value(); if (*tape) jmp();})
Inst(Jz, {value(); if (!*tape) jmp();})
Inst(Set0, {*tape=0;})
Inst(Set1, {*tape=1;})
Inst(Set, {value(); *tape = o->off;})
Inst(Return, {goto out;})
