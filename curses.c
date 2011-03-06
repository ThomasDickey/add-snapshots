/******************************************************************************
 * Copyright 1995-2010,2011 by Thomas E. Dickey                               *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/

static const char Id[] = "$Id: curses.c,v 1.17 2011/03/06 16:58:53 tom Exp $";

/*
 * Title:	curses.c
 * Author:	T.E.Dickey
 * Created:	14 Dec 1995 (split-off from add.c)
 * Modified:
 *
 * Function:	This module hides the curses functional interface from 'add'
 *
 */

#include <add.h>
#include <screen.h>

#include <stdarg.h>

#if HAVE_XCURSES
#include <xcurses.h>
#else
#include <curses.h>
#endif

#ifndef PADSLASH		/* PDCURSES */
#define PADSLASH '/'
#define PADSTAR  '*'
#define PADPLUS  '+'
#define PADMINUS '-'
#define PADENTER '\n'
#endif

#if !HAVE_COLOR_PAIR && defined(COLOR_PAIR)
#define HAVE_COLOR_PAIR 1
#endif

#if HAVE_COLOR_PAIR
#define	CURRENT_COLOR current_color
#define SetColors(n)  wattron(stdscr, current_color = COLOR_PAIR(n))
#else
#define CURRENT_COLOR 0
#define SetColors(n)
#endif

#ifdef A_BOLD
#define BeginBold()  wattron (stdscr, A_BOLD    | CURRENT_COLOR)
#define EndOfBold()  wattroff(stdscr, A_BOLD)
#else
#define BeginBold()  standout()
#define EndOfBold()  standend()
#endif

#ifdef A_REVERSE
#define BeginHigh()  wattron (stdscr, A_REVERSE | CURRENT_COLOR)
#define EndOfHigh()  wattroff(stdscr, A_REVERSE)
#else
#define BeginHigh()  standout()
#define EndOfHigh()  standend()
#endif

/*
 * Common data
 */
Bool screen_active;		/* true while we've got curses running */
int screen_full;		/* screen-size */
int screen_half;		/* scrolling amount */

#if HAVE_COLOR_PAIR
static chtype current_color;
#endif

int
is_delete_left(int c)
{
    switch (c) {
    case '\b':
    case '\177':
#if defined(KEY_BACKSPACE)
    case KEY_BACKSPACE:
#endif
#if defined(KEY_DC)
    case KEY_DC:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_down_char(int c)
{
    switch (c) {
    case VI_DOWN:
#ifdef KEY_DOWN
    case KEY_DOWN:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_down_page(int c)
{
    switch (c) {
    case VI_NPAGE:
#ifdef KEY_NPAGE
    case KEY_NPAGE:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_end_char(int c)
{
    switch (c) {
    case CTL('E'):
#ifdef KEY_END
    case KEY_END:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_left_char(int c)
{
    switch (c) {
    case CTL('B'):		/* only used in editing comments */
    case VI_LEFT:
#ifdef KEY_LEFT
    case KEY_LEFT:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_home_char(int c)
{
    switch (c) {
    case CTL('A'):
#ifdef KEY_HOME
    case KEY_HOME:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_right_char(int c)
{
    switch (c) {
    case CTL('F'):		/* only used in editing comments */
    case VI_RIGHT:
#ifdef KEY_RIGHT
    case KEY_RIGHT:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_up_char(int c)
{
    switch (c) {
    case VI_UP:
#ifdef KEY_UP
    case KEY_UP:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

Bool
is_up_page(int c)
{
    switch (c) {
    case VI_PPAGE:
#ifdef KEY_PPAGE
    case KEY_PPAGE:
#endif
	return TRUE;
    default:
	return FALSE;
    }
}

/*
 * Beep (or flash, preferably) when we get a minor error.
 */
void
screen_alarm(void)
{
#if HAVE_FLASH
    flash();
#elif HAVE_BEEP
    beep();
#else
    (void) write(2, "\007", 1);	/* Real BSD-curses has 'beep()' */
#endif
}

void
screen_clear_bottom(void)
{
    clrtobot();
}

void
screen_clear_endline(void)
{
    clrtoeol();
}

/*
 * Returns current column
 */
int
screen_col(void)
{
    int y, x;

    getyx(stdscr, y, x);
    return x;
}

/*
 * Returns the columns left on the line, given a starting column
 */
int
screen_cols_left(int col)
{
    return COLS - 1 - col;
}

/*
 * Delete the character at the current position
 */
void
screen_delete_char(void)
{
    delch();
}

void
screen_finish(void)
{
    if (screen_active) {
	nl();			/* does no harm, some curses don't do it */
	endwin();
	screen_active = FALSE;	/* flag showing that curses is off */
    }
}

/*
 * Read a character from the terminal
 */
int
screen_getc(void)
{
    int c;

    refresh();
    c = getch();
    switch (c) {
    case PADSLASH:
	c = OP_DIV;
	break;
    case PADSTAR:
	c = OP_MUL;
	break;
    case PADPLUS:
	c = OP_ADD;
	break;
    case PADMINUS:
	c = OP_SUB;
	break;
    case PADENTER:
	c = '\n';
	break;
    }
    return (c);
}

/*
 * Insert a character at the current position
 */
void
screen_insert_char(int c)
{
    insch((unsigned char) c);
}

/*
 * Show text in the status line
 */
void
screen_message(const char *format,...)
{
    char msg[BUFSIZ];
    va_list ap;
    int y, x;

    getyx(stdscr, y, x);
    move(0, 0);

    va_start(ap, format);
    vsprintf(msg, format, ap);
    va_end(ap);
    addstr(msg);

    clrtoeol();
    move(y, x);
    refresh();
}

/*
 * Move left within the given buffer.
 */
int
screen_move_left(int col, int limit)
{
    if (col > limit) {
	register int y, x;
	getyx(stdscr, y, x);
	move(y, x - 1);
	col--;
    } else
	screen_alarm();
    return col;
}

/*
 * Move right within the given buffer.
 */
int
screen_move_right(int col, int limit)
{
    if (col < limit) {
	register int y, x;
	getyx(stdscr, y, x);
	move(y, x + 1);
	col++;
    } else
	screen_alarm();
    return col;
}

void
screen_printf(const char *format,...)
{
    char msg[BUFSIZ];
    va_list ap;

    va_start(ap, format);
    vsprintf(msg, format, ap);
    va_end(ap);
    addstr(msg);
}

void
screen_putc(int c)
{
    addch((unsigned char) c);
}

void
screen_puts(const char *string)
{
    addstr(string);
}

/*
 * Returns current row
 */
int
screen_row(void)
{
    int y, x;

    getyx(stdscr, y, x);
    return y;
}

/*
 * Returns the rows left on the line, given a starting row
 */
int
screen_rows_left(int row)
{
    return LINES - 1 - row;
}

void
screen_set_bold(Bool flag)
{
    if (flag)
	BeginBold();
    else
	EndOfBold();
}

void
screen_set_position(int row, int column)
{
    move(row, column);
}

void
screen_set_reverse(Bool flag)
{
    if (flag)
	BeginHigh();
    else
	EndOfHigh();
}

/*
 * Setup and run the interactive portion of the program.
 */
void
screen_start(void)
{
    if (initscr() == 0)		/* should return a "WINDOW *" */
	exit(EXIT_FAILURE);
#if HAVE_KEYPAD
    keypad(stdscr, TRUE);
#endif
#if HAVE_DEFINE_KEY
    define_key("\033Ok", '+');
    define_key("\033Ol", ',');
    define_key("\033Om", '-');
    define_key("\033On", '.');
    define_key("\033Op", '0');
    define_key("\033Oq", '1');
    define_key("\033Or", '2');
    define_key("\033Os", '3');
    define_key("\033Ot", '4');
    define_key("\033Ou", '5');
    define_key("\033Ov", '6');
    define_key("\033Ow", '7');
    define_key("\033Ox", '8');
    define_key("\033Oy", '9');
    define_key("\033OM", '\n');
#endif
#if defined(COLOR_BLUE) && defined(COLOR_WHITE) && HAVE_COLOR_PAIR
    if (has_colors()) {
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);	/* normal */
	SetColors(1);
#if HAVE_BKGD
	bkgd(CURRENT_COLOR);
#endif
    }
#endif
#if HAVE_TYPEAHEAD
    typeahead(-1);		/* disable typeahead */
#endif
#if SYS_MSDOS && defined(F_GRAY) && defined(B_BLUE)
    wattrset(stdscr, F_GRAY | B_BLUE);	/* patch for old PD-Curses */
#endif
    raw();
    nonl();
    noecho();
    screen_full = LINES - 1;
    screen_half = (screen_full + 1) / 2;
    screen_active = TRUE;

#if HAVE_WSETSCRREG
    setscrreg(2, screen_full);
#endif
}
