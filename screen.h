/******************************************************************************
 * Copyright 1995-2007,2020 by Thomas E. Dickey                               *
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

/* $Id: screen.h,v 1.6 2020/09/22 19:31:50 tom Exp $ */
#ifndef SCREEN_H
#define SCREEN_H

extern Bool screen_active;
extern int screen_half;
extern int screen_full;

extern Bool is_delete_left(int c);
extern Bool is_down_char(int c);
extern Bool is_down_page(int c);
extern Bool is_end_char(int c);
extern Bool is_home_char(int c);
extern Bool is_left_char(int c);
extern Bool is_right_char(int c);
extern Bool is_up_char(int c);
extern Bool is_up_page(int c);
extern void screen_alarm(void);
extern void screen_clear_bottom(void);
extern void screen_clear_endline(void);
extern int screen_col(void);
extern int screen_cols_left(int col);
extern void screen_delete_char(void);
extern void screen_finish(void);
extern int screen_getc(void);
extern void screen_insert_char(int c);
extern void screen_message(const char *format, ...);
extern int screen_move_left(int column, int limit);
extern int screen_move_right(int column, int limit);
extern void screen_printf(const char *format, ...);
extern void screen_putc(int c);
extern void screen_puts(const char *string);
extern int screen_row(void);
extern int screen_rows_left(int row);
extern void screen_set_bold(Bool flag);
extern void screen_set_position(int row, int column);
extern void screen_set_reverse(Bool flag);
extern void screen_start(void);

#endif /* SCREEN_H */
