/* $Id: add.h,v 1.3 1995/12/10 20:29:55 tom Exp $
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

#include	<curses.h>

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

#if !SYS_MSDOS
#define PADSLASH '/'
#define PADSTAR  '*'
#define PADPLUS  '+'
#define PADMINUS '-'
#define PADENTER '\n'
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

#ifdef	COLOR_PAIR
#define	CURRENT_COLOR current_color
#define SetColors(n)  wattron(stdscr, current_color = COLOR_PAIR(n))
#else
#define CURRENT_COLOR 0
#define SetColors(n)
#endif

#ifdef  A_BOLD
#define BeginBold()  wattron (stdscr, A_BOLD    | CURRENT_COLOR)
#define EndOfBold()  wattroff(stdscr, A_BOLD)
#else
#define BeginBold()  standout()
#define EndOfBold()  standend()
#endif

#ifdef  A_REVERSE
#define BeginHigh()  wattron (stdscr, A_REVERSE | CURRENT_COLOR)
#define EndOfHigh()  wattroff(stdscr, A_REVERSE)
#else
#define BeginHigh()  standout()
#define EndOfHigh()  standend()
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

#define isKey(c,code)  ((c) == code)

#ifdef KEY_UP
# define isMoveUp(c)    ((c) == VI_UP    || isKey(c, KEY_UP))
#else
# define isMoveUp(c)    ((c) == VI_UP)
#endif

#ifdef KEY_DOWN
# define isMoveDown(c)  ((c) == VI_DOWN  || isKey(c, KEY_DOWN))
#else
# define isMoveDown(c)  ((c) == VI_DOWN)
#endif

#ifdef KEY_LEFT
# define isMoveLeft(c)  ((c) == VI_LEFT  || isKey(c, KEY_LEFT))
#else
# define isMoveLeft(c)  ((c) == VI_LEFT)
#endif

#ifdef KEY_RIGHT
# define isMoveRight(c) ((c) == VI_RIGHT || isKey(c, KEY_RIGHT))
#else
# define isMoveRight(c) ((c) == VI_RIGHT)
#endif

#ifdef KEY_PPAGE
# define isPageUp(c)    ((c) == VI_PPAGE || isKey(c, KEY_PPAGE))
#else
# define isPageUp(c)    ((c) == VI_PPAGE)
#endif

#ifdef KEY_NPAGE
# define isPageDown(c)  ((c) == VI_NPAGE || isKey(c, KEY_NPAGE))
#else
# define isPageDown(c)  ((c) == VI_NPAGE)
#endif

#define isAscii(c)	((c) < 128)	/* isascii isn't portable */
#define isReturn(c)     ((c) == '\r' || (c) == '\n')
#define isDigit(c)      (isAscii(c) && isdigit(c))

#define LastData(np)    ((np)->next == 0)

	/* Operators */
#define	OP_ADD	'+'
#define	OP_SUB	'-'
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
