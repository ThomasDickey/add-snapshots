/******************************************************************************
 * Copyright 1995 by Thomas E. Dickey.  All Rights Reserved.                  *
 *                                                                            *
 * You may freely copy or redistribute this software, so long as there is no  *
 * profit made from its use, sale trade or reproduction. You may not change   *
 * this copyright notice, and it must be included in any copy made.           *
 ******************************************************************************/
/* $Id: screen.h,v 1.4 1995/12/25 21:42:18 tom Exp $ */
extern	Bool	screen_active;
extern	int	screen_half;
extern	int	screen_full;

extern	Bool	is_delete_left(int c);
extern	Bool	is_down_char(int c);
extern	Bool	is_down_page(int c);
extern	Bool	is_end_char(int c);
extern	Bool	is_home_char(int c);
extern	Bool	is_left_char(int c);
extern	Bool	is_right_char(int c);
extern	Bool	is_up_char(int c);
extern	Bool	is_up_page(int c);
extern	void	screen_alarm(void);
extern	void	screen_clear_bottom(void);
extern	void	screen_clear_endline(void);
extern	int	screen_col(void);
extern	int	screen_cols_left(int col);
extern	void	screen_delete_char(void);
extern	void	screen_finish(void);
extern	int	screen_getc(void);
extern	void	screen_insert_char(int c);
extern	void	screen_message(const char *format, ...);
extern	int	screen_move_left(int column, int limit);
extern	int	screen_move_right(int column, int limit);
extern	void	screen_printf(const char *format, ...);
extern	void	screen_putc(int c);
extern	void	screen_puts(const char *string);
extern	int	screen_row(void);
extern	int	screen_rows_left(int row);
extern	void	screen_set_bold(Bool flag);
extern	void	screen_set_position(int row, int column);
extern	void	screen_set_reverse(Bool flag);
extern	void	screen_start(void);
