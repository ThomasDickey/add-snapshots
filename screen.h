/******************************************************************************
 * Copyright 1995-2020,2024 by Thomas E. Dickey                               *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL   *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR   *
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,      *
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR      *
 * OTHER DEALINGS IN THE SOFTWARE.                                            *
 *                                                                            *
 * Except as contained in this notice, the name(s) of the above copyright     *
 * holders shall not be used in advertising or otherwise to promote the sale, *
 * use or other dealings in this Software without prior written               *
 * authorization.                                                             *
 ******************************************************************************/

/* $Id: screen.h,v 1.7 2024/09/16 00:02:10 tom Exp $ */
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
