/******************************************************************************
 * Copyright 1995 by Thomas E. Dickey.  All Rights Reserved.                  *
 *                                                                            *
 * You may freely copy or redistribute this software, so long as there is no  *
 * profit made from its use, sale trade or reproduction. You may not change   *
 * this copyright notice, and it must be included in any copy made.           *
 ******************************************************************************/
/* $Id: add.h,v 1.9 2002/12/29 20:01:38 tom Exp $
 *
 * common definitions for 'add' utility
 */

#ifndef ADD_H
#define ADD_H

#if defined(vaxc) || defined(vms)
# define SYS_VMS 1
#elif defined(__TURBOC__)
# define SYS_MSDOS 1
# define HAVE_GETOPT_H 1	/* GNU library */
#else
# define SYS_UNIX 1
# include <config.h>	/* generated by 'configure' */
#endif

#ifndef HAVE_BKGD
#define HAVE_BKGD 0
#endif

#ifndef HAVE_COLOR_PAIR
#define HAVE_COLOR_PAIR 0
#endif

#ifndef HAVE_DBMALLOC_H
#define HAVE_DBMALLOC_H 0
#endif

#ifndef HAVE_FLASH
#define HAVE_FLASH 0
#endif

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif

#ifndef HAVE_GETOPT_H
#define HAVE_GETOPT_H 0
#endif

#ifndef HAVE_KEYPAD
#define HAVE_KEYPAD 0
#endif

#ifndef HAVE_TYPEAHEAD
#define HAVE_TYPEAHEAD 0
#endif

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H 0
#endif

#ifndef HAVE_WSETSCRREG
#define HAVE_WSETSCRREG 0
#endif

#ifndef HAVE_XCURSES
#define HAVE_XCURSES 0
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#ifndef SYS_MSDOS
#define SYS_MSDOS 0
#endif

#ifndef SYS_UNIX
#define SYS_UNIX 0
#endif

#ifndef SYS_VMS
#define SYS_VMS 0
#endif

#include	<stdlib.h>

#if HAVE_UNISTD_H
#include	<unistd.h>
#endif

#include	<stdio.h>
#include	<ctype.h>
#include	<errno.h>
#include	<string.h>
#include	<math.h>
#include	<signal.h>
#include	<sys/types.h>
#include	<sys/stat.h>

#if HAVE_GETOPT
# if HAVE_GETOPT_H
#  include <getopt.h>
# else
   extern char	*optarg;
   extern int	optind;
# endif
#endif

#if HAVE_DBMALLOC_H
#include	<dbmalloc.h>
#define NO_LEAKS 1
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#if SYS_UNIX
#define PATHSEP ':'
#endif

#if SYS_VMS
#define PATHSEP ','
#define A_BOLD _BOLD
#define KEY_UP    SMG$K_TRM_UP
#define KEY_DOWN  SMG$K_TRM_DOWN
#define KEY_LEFT  SMG$K_TRM_LEFT
#define KEY_RIGHT SMG$K_TRM_RIGHT
#define KEY_NPAGE SMG$K_TRM_PF3
#define KEY_PPAGE SMG$K_TRM_PF4
#define wattrset(w,a) wsetattr(w,a)
#define wattrclr(w,a) wclrattr(w,a)
#define HAVE_BEEP 1
#endif

#if SYS_MSDOS
#define PATHSEP ';'
#include <io.h>
#define HAVE_BEEP 1
#define HAVE_KEYPAD 1
#endif

/*
 * Local declarations:
 */
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define	MAXPATH	256
#define	MAXBFR	132

#undef  SIZEOF
#define	SIZEOF(v)	(sizeof(v)/sizeof(v[0]))

	/* Tests for special character types */
#define	CTL(c)          ((c) & 0x1f)
#define	VI_UP           'k'
#define VI_DOWN         'j'
#define VI_LEFT         'h'
#define VI_RIGHT        'l'
#define VI_PPAGE        CTL('B')
#define VI_NPAGE        CTL('F')

#define isAscii(c)	((c) < 128)	/* isascii isn't portable */
#define isReturn(c)     ((c) == '\r' || (c) == '\n')
#define isDigit(c)      (isAscii(c) && isdigit(c))

#define LastData(np)    ((np)->next == 0)

	/* Operators */
#define	OP_ADD	'+'
#define	OP_SUB	'-'
#define	OP_NEG	'~'
#define	OP_MUL	'*'
#define	OP_DIV	'/'
#define	OP_INT	'%'
#define	OP_TAX	'$'

#define	DefaultOp(np) np->cmd

	/* Miscellaneous characters */
#define	L_PAREN	'('
#define	R_PAREN	')'

#define	EQUALS	'='
#define	PERIOD	'.'
#define	COMMA	','
#define COLON   ':'

#define	EOS	'\0'

/*
 * Local types
 */
typedef	int	Bool;
typedef	double	Value;		/* provides more precision than 'long' */

#define	DATA	struct _oprs
	DATA {
	DATA	*next;
	DATA	*prev;
	char	*txt;		/* comment, if any */
	char	cmd;		/* operator */
	Bool	psh;		/* true if we use left paren instead of 'val' */
	int	dot;		/* number of digits past '.' */
	Value	val;		/* operand value */
	Value	sum;		/* running total */
	Value	aux;		/* auxiliary value (derived from 'val') */
	};

#endif /* ADD_H */
