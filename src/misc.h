/*
 * Modified by Max Satula to keep only functionality needed for ocp program
 */

#ifndef _MISC_H
#define _MISC_H

time_t	 monotime(void); /* taken from original OpenSSH misc.h */
size_t strlcat(char *dst, const char *src, size_t siz); /* declaration for strlcat.c */

#endif /* _MISC_H */
