/*
 * This file has not just been taken from OpenSSH, because the original one
 * had a lot of declarations unused in ocp
 * Instead, this file has been generated from scratch based on source files,
 * pulling the only functionality really needed to compile ocp program.
 */

#ifndef _MISC_H
#define _MISC_H

double
monotime_double(void)
; /* taken from the original OpenSSH misc.h/misc.c */
size_t
strlcat(char *dst, const char *src, size_t dsize)
; /* declaration for strlcat.c */

#endif /* _MISC_H */
