static const char Id[] = "$Id: add.c,v 1.9 1995/12/10 22:46:51 tom Exp $";
static const char copyrite[] = "ADD V1.1 -- Copyright 1993 by Thomas E. Dickey";

/*
 * Title:	add.c
 * Author:	T.E.Dickey
 * Created:	05 May 1986
 * Modified:
 *		10 Dec 1995, use 'autoconf' script.
 *		02 Apr 1994, fixes to 'ShowValue()' to show result bold.
 *		24 Oct 1993, revised to work with PD Curses 2.1 and Turbo C/C++ 3.0,
 *			     and builtin help-screen.
 *
 * Function:	This is a simple adding machine that uses curses to display
 *		a column of values, operators and results.  The user can
 *		move up and down in the column, modifying the values and
 *		operators.
 *
 */

#include <add.h>

static	void	Recompute (DATA *);
static	void	ShowRange (DATA *, DATA *);
static	void	ShowFrom  (DATA *);

/*
 * Common data
 */
static	char	*top_output;
static	DATA	*all_data,	/* => beginning of data */
		*top_data;	/* => beginning of current screen */

static  Value	val_frac;      	/* # of units in 'len_frac' (e.g., 100.0) */
static	long	big_long;	/* largest signed 'long' value */
static	int	interval,	/* compounding interval-divisor */
		half,		/* scrolling amount */
		full,		/* screen-size */
		val_width,	/* maximum width of formatted number */
		len_frac;	/* nominal number of digits after period */
static	Bool	cursed,		/* true while we've got curses running */
		show_error;	/* suppress normal reporting until GetC() */
#ifdef	COLOR_PAIR
static	chtype	current_color;
#endif

/*
 * Input-script control:
 */
static	FILE	*scriptFP;	/* current script file-pointer */
static  char	**scriptv;	/* pointer to list of input-scripts */
static	Bool	scriptCHG;	/* set true if there's a change after scripts */
static	Bool	scriptNUM;	/* set true to 0..n for script number */

/*
 * Help-screen
 */
static	DATA	*all_help;
static	char	*helpfile;

/*
 * Check to see if the given character is a legal 'add' operator (excluding
 * editing/scrolling):
 */
static	struct	{
	char	command;
	char	repeats;
	char	toggles;
	Bool	isunary;
	char *	explain;
	} Commands [] = {
		{OP_ADD,  'a',	'A',	TRUE,	"add"},
		{OP_SUB,  's',	'S',	TRUE,	"subtract"},
		{OP_MUL,  'm',	'M',	FALSE,	"multiply"},
		{OP_DIV,  'd',	'D',	FALSE,	"divide"},
		{OP_INT,  'i',	'I',	FALSE,	"interest"},
		{OP_TAX,  't',	'T',	FALSE,	"tax"},
		{L_PAREN, EOS,	EOS,    TRUE,	"begin group"},
		{R_PAREN, EOS,	EOS,    FALSE,	"end group"}
	};

/*
 * Normally we don't show the results of replaying a script. This makes
 * loading scripts much faster.
 */
static
int	isVisible(void)
{
	return (scriptFP == 0);
}

/*
 * Beep (or flash, preferably) when we get a minor error.
 */
static
void	Alarm(void)
{
#if HAVE_FLASH
	flash();
#elif HAVE_BEEP
	beep();
#else
	(void) write(2, "\007", 1);	/* Real BSD-curses has 'beep()' */
#endif
}

/*
 * Returns current column
 */
static
int	CurrentCol(void)
{
	int	y, x;
	getyx(stdscr, y, x);
	return x;
}

/*
 * Lookup a character to see if it is a legal operator, returning nonnull
 * in that case.
 */
#define	LOOKUP(func,lookup,result) \
	static	int	func(int c) { \
			register int j; \
			for (j = 0; j < SIZEOF(Commands); j++) \
				if (Commands[j].lookup == c) \
					return result; \
			return 0; \
			}

LOOKUP(isCommand,command,c)
LOOKUP(isRepeats,repeats,Commands[j].command)
LOOKUP(isToggles,toggles,Commands[j].command)
LOOKUP(isUnary,command,  Commands[j].isunary)

static
int	isDelete(int c)
{
	switch (c) {
	case '\b':
	case '\177':
#if  defined(KEY_BACKSPACE)
	case KEY_BACKSPACE:
#endif
#if  defined(KEY_DC)
	case KEY_DC:
#endif
		return TRUE;
	default:
		return FALSE;
	}
}
/*
 * Find the end of the DATA list
 */
static
DATA *	EndOfData(void)
{
	register DATA *np;
	if ((np = all_data) != 0) {
		while (!LastData(np))
			np = np->next;
	}
	return np;
}

/*
 * Returns true if the DATA entry is either the 0th or 1st entry in the
 * list.
 */
static
int	FirstData (DATA *np)
{
	return (np->prev == 0 || np->prev == all_data);
}

/*
 * Tests for special case of operators that cannot appear in a unary context.
 */
static
int	UnaryConflict (DATA *np, int chr)
{
	if (isCommand(chr))
		return (!isUnary(chr) && (FirstData(np) || np->prev->psh));
	return FALSE;
}

/*
 * Trim whitespace from the end of a string
 */
static
void	TrimString (char *src)
{
	register char	*end = src + strlen(src);

	while (end-- != src) {
		if (isspace(*end))
			*end = EOS;
		else
			break;
	}
}

/*
 * Allocate a string, trimming whitespace from the end for consistency.
 */
static
char *	AllocString (char *src)
{
	return strcpy(malloc((unsigned)(strlen(src)+1)), src);
}

/*
 * Allocate and initialize a DATA entry
 */
static
DATA *	AllocData (DATA *after)
{
	register DATA *np = (DATA *)malloc(sizeof(DATA));

	np->txt = 0;
	np->val =
	np->sum =
	np->aux = 0.0;
	np->cmd = EOS;
	np->psh = FALSE;
	np->dot = len_frac;
	np->next =
	np->prev = 0;

	if (after != 0) {
		np->prev = after;
		np->next = after->next;
		after->next = np;
		if (!LastData(np))
			np->next->prev = np;
	} else {	/* append to the end of the list */
		register DATA *op = EndOfData();
		if (op != 0) {
			op->next = np;
			np->prev = op;
		} else {
			all_data = np;
		}
	}
	return np;
}

/*
 * Free and delink the data from the list.  If it was a permanent entry,
 * recompute the display from that point.  Otherwise, simply repaint.
 */
static
DATA *	FreeData (DATA *np, int permanent)
{
	register DATA *prev = np->prev;
	register DATA *next = np->next;

	if (prev == 0) {	/* we're at the beginning of the list */
		all_data = next;
		if (next != 0)
			next->prev = 0;
	} else {
		prev->next = next;
		if (next != 0) {
			next->prev = prev;
		} else {	/* deleted the end-of-data */
			next = prev;
		}
	}
	if (np->txt != 0)
		free(np->txt);
	free((char *)np);

	if (top_data == np)
		top_data = next;

	if (cursed) {
		if (permanent) {
			Recompute(next);
		} else {
			ShowFrom(next);
		}
	}
	return next;
}

/*
 * Count the data from the beginning to the specified entry.
 */
static
int	CountData (DATA *np)
{
	register int seq = 0;
	while (np->prev != 0) {
		seq++;
		np = np->prev;
	}
	return seq;
}

static
int	CountFromTop (DATA *np)
{
	return CountData(np) - CountData(top_data);
}

static
int	CountAllData(void)
{
	return CountData(EndOfData());
}

static
DATA *	FindData (int seq)
{
	register DATA *np = all_data;
	while (seq-- > 0) {
		if (np == 0)
			break;
		np = np->next;
	}
	return np;
}

/*
 * Remove any fractional portion of the given value 'val', return the result.
 */
static
Value	Floor (Value val)
{
	if (val > big_long) {
		val = big_long;
	} else if (val < -big_long) {
		val = -big_long;
	} else {
		long tmp = val;
		val = tmp;
	}
	return (val);
}

static
Value	Ceiling (Value val)
{
	Value tmp = Floor(val);

	if (val > 0.0 && val > tmp) {
		tmp += 1.0;
	} else if (val < 0.0 && val < tmp) {
		tmp -= 1.0;
	}
	return (tmp);
}

/*
 * Return the last operand for the given operator, excluding the given data.
 */
static
Value	LastVAL (DATA *np, int cmd)
{
	np = np->prev;
	while (np != 0) {
		if (cmd == np->cmd)
			return (np->val);
		np = np->prev;
	}
	if (cmd == OP_MUL || cmd == OP_DIV)
		return (1.0 * val_frac);
	else if (cmd == OP_INT || cmd == OP_TAX)
		return (4.0 * val_frac);
	return (0.0);
}

/*
 * Convert the given value 'val' to printing format (comma between groups of
 * three digits, decimal point before fractional part).
 */
static
char *	Format (char *dst, Value val)
{
	int	len, j, neg = val < 0.0;
	size_t	grp;
	char	bfr[MAXBFR],
		*s = dst;

	if (neg) {
		val = -val;
		*s++ = OP_SUB;
	}

	if (val >= big_long) {
		(void) strcpy (s, " ** overflow");
	} else {
		(void) sprintf (bfr, "%0*.0f", len_frac, val);
		len = strlen(bfr) - len_frac;
		grp = len % 3;
		j   = 0;

		while (j < len) {
			if (grp) {
				(void) strncpy (s, &bfr[j], grp);
				j += grp;
				s += grp;
				if (j < len) *s++ = COMMA;
			}
			grp = 3;
		}
		(void) sprintf (s, ".%s", &bfr[len]);
	}
	return (dst);
}

/*
 * Convert a value to printing form, writing it on the screen:
 */
static
void	putval (Value val)
{
	char	bfr[MAXBFR];

	(void) printw ("%*.*s", val_width, val_width, Format(bfr, val));
}

/*
 */
static
void	setval (DATA *np, int cmd, Value val, int psh)
{
	np->cmd = isCommand(cmd) ? cmd : OP_ADD;
	np->val = val;
	np->psh = psh;
}

/*
 * Compute the parenthesis level of the given data entry.  This is a positive
 * number.
 */
static
int	LevelOf (DATA *target)
{
	register DATA *np;
	register level = 0;

	for (np = all_data; np != target; np = np->next) {
		if (np->cmd == R_PAREN)	level--;
		if (np->psh)		level++;
	}
	return level;
}

/*
 * Return a pointer to the last data entry in the screen.
 */
static
DATA *	ScreenBottom(void)
{
	register DATA *np = top_data;
	register int  count = full;

	while (--count > 0) {
		if (!LastData(np))
			np = np->next;
		else
			break;
	}
	return np;
}

/*
 * For an entry that isn't the currently-edited one, show the operator,
 * operand and result(s).  Return the index of this entry in the screen
 * to use in testing for completion of repainting activity.
 */
static
int	ShowValue (DATA *np, int *editing, Bool comment)
{
	int row = CountFromTop(np);
	int col = 0;
	int level;

	if (!isVisible()) {
		if (editing != 0) {
			editing[0] =
			editing[1] = 0;
		}
	} else if (row >= 0 && row < full) {
		char	cmd = isprint(np->cmd) ? np->cmd : '?';

		move(row+1, col);
		clrtoeol();
		if (np->cmd != EOS) {
			for (level = LevelOf(np); level > 0; level--)
				addstr(". ");
			if (editing != 0) {
				BeginHigh();
				(void) printw(" %c>>", cmd);
				EndOfHigh();
			} else {
				(void) printw(" %c: ", cmd);
			}

			if (editing != 0) {
				*editing = CurrentCol();
				BeginHigh();
			}

			if ((cmd == R_PAREN) || ((editing != 0) && !comment))
				(void) printw ("%*.*s", val_width, val_width, " ");
			else if (np->psh)
				addch(L_PAREN);
			else
				putval(np->val);

			if (editing != 0)
				EndOfHigh();

			if (!np->psh) {
				addstr(" ");
				if (editing != 0) BeginBold();
				putval(np->sum);
				if (editing != 0) EndOfBold();
			}
			if (cmd == OP_INT || cmd == OP_TAX) {
				addstr(" ");
				putval(np->aux);
			}

			col = CurrentCol();
			if (editing != 0)
				editing[1] = col;
			col += 4;
			if ((np->txt != 0) && (col < COLS))
				addstr(" # ");
		}

		if ((np->txt != 0) && (col < COLS)) {
			(void) printw("%.*s", COLS - col - 1, np->txt);
		}
		if (LastData(np) && (row < LINES-2)) {
			move(row+2, 0);
			clrtobot();
		}
	}
	return row;
}

static
void	ShowRange (DATA *first, DATA *last)
{
	register DATA *np = first;
	while (np != last) {
		if (ShowValue(np, (int *)0, FALSE) >= full)
			break;
		np = np->next;
	}
}

static
void	ShowFrom (DATA *first)
{
	ShowRange(first, (DATA *)0);
}

/*
 * (Re)display the status line at the top of the screen.
 */
static
void	ShowStatus (DATA *np, int opened)
{
	int	seq = CountData(np);
	int	top = CountData(top_data);
	DATA	*last = EndOfData();
	register int j, c;
	char	buffer[BUFSIZ];

	if (!show_error && isVisible()) {
		BeginBold();
		move (0,0);
		clrtoeol();
		(void) sprintf (buffer, "%d of %d", seq, CountData(last));
		move (0, COLS-((int)strlen(buffer)+1));
		addstr(buffer);
		move (0,0);
		if (opened < 0) {
			addstr("Edit comment (press return to exit)");
		} else if (opened > 0) {
			addstr("Open-line expecting ");
			for (j = 0; j < SIZEOF(Commands); j++) {
				if ((c = Commands[j].command) == L_PAREN)
					continue;
				if ((c == R_PAREN) && (opened < 2))
					continue;
				addch((chtype)c);
			}
		} else if (np->cmd != EOS) { /* editing a value */
			for (j = 0; j < SIZEOF(Commands); j++) {
				if (Commands[j].command == np->cmd) {
					(void) printw("  %s", Commands[j].explain);
					break;
				}
			}
			move (0, 5 + val_width);
			putval(last->sum);
			addstr(" -- total");
		}
		EndOfBold();
	}
	if (isVisible())
		move(seq-top+1,0);
}

/*
 * Show text in the status line
 */
static
void	ShowInfo (char *msg)
{
	if (cursed) {	/* we've started curses */
		int y, x;
		getyx(stdscr, y, x);
		move(0,0);
		addstr(msg);
		clrtoeol();
		move(y,x);
		refresh();
	} else {
		(void) printf("%s\n", msg);
	}
}

/*
 * Show an error-message in the status line
 */
static
void	ShowError (char *msg, char *arg)
{
	static	char	format[] = "?? %s \"%s\"";

	if (cursed) {	/* we've started curses */
		char	temp[BUFSIZ];
		(void) sprintf(temp, format, msg, arg);
		ShowInfo(temp);
		show_error = TRUE;
	} else {
		(void) fprintf(stderr, format, msg, arg);
		(void) fprintf(stderr, "\n");
		perror("add");
		exit(errno);
	}
}

/*
 * Returns true if a file exists, -true if it isn't a file, false if neither.
 */
static
int	Fexists (char *path)
{
	struct	stat	sb;
	if (*path == EOS)
		ShowError("No filename specified", path);
	if (stat(path, &sb) < 0)
		return FALSE;
	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		ShowError("Not a file", path);
		return -TRUE;
	}
	return TRUE;
}

/*
 * Check file-access for writing a script
 */
static
int	Ok2Write (char *path)
{
	if (Fexists(path) != -TRUE
	 && access(path, 02) != 0
	 && errno != ENOENT) {
		ShowError("No write access", path);
	} else {
		return TRUE;
	}
	return FALSE;
}

/*
 * Write the current list of data as an ADD-script
 */
static
void	PutScript (char *path)
{
	DATA	*np;
	FILE	*fp = fopen(path, "w");
	char	buffer[MAXBFR];

	if (fp == 0)
		ShowError("Cannot open output", path);

	(void) sprintf (buffer, "Writing results to \"%s\"", path);
	ShowInfo(buffer);

	for (np = all_data->next; np != 0; np = np->next) {
		if (np->cmd == EOS && np->next == 0)
			break;
		(void) fprintf (fp, "%c", np->cmd);
		if (np->psh)
			(void) fprintf (fp, "%c", L_PAREN);
		else if (np->cmd != R_PAREN)
			(void) fprintf (fp, "%s", Format(buffer, np->val));
		if (!np->psh)
			(void) fprintf (fp, "\t%s", Format(buffer, np->sum));
		if (np->cmd == OP_INT
		 || np->cmd == OP_TAX)
			(void) fprintf(fp, "\t%s", Format(buffer, np->aux));
		if (np->txt != 0)
			(void) fprintf(fp, "\t#%s", np->txt);
		(void) fprintf (fp, "\n");
	}
	(void) fclose (fp);

	/* If we've written the specified output, reset the changed-flag */
	if (!strcmp(path, top_output))
		scriptCHG = FALSE;
}

/*
 * Check file-access for reading a script.
 */
static
int	Ok2Read (char *path)
{
	if (scriptFP != 0)
		ShowError("Cannot nest scripts", path);
	else if (Fexists(path) != TRUE || access(path, 04) != 0)
		ShowError("No read access", path);
	else
		return TRUE;
	return FALSE;
}

/*
 * As long as there is another input-script to process, read it.  Scripts are
 * formatted
 *	<operator><value><tab><ignored>
 * to permit line-oriented entries.
 */
static
int	GetScript(void)
{
	static	int	first;
	static	int	ignored;
	static	int	comment;
	register int c;

	while (*scriptv != NULL) {
		int was_invisible = !isVisible();

		if (scriptFP == 0) {
			scriptFP = fopen(*scriptv, "r");
			if (scriptFP == 0) {
				ShowError("Cannot read", *scriptv);
				scriptv++;
			} else {
				ShowInfo("Reading script");
				first = TRUE;
			}
			continue;
		}
		while (scriptFP != 0) {
			c = fgetc(scriptFP);
			if (feof(scriptFP) || ferror(scriptFP)) {
				(void) fclose (scriptFP);
				scriptFP = NULL;
				scriptv++;
				if (!scriptNUM++)
					scriptCHG = FALSE;
				continue;
			}
			if (c == '#' || c == COLON) {
				comment = TRUE;
				ignored = FALSE;
			} else if (!comment && (c == '\t')) {
				ignored = TRUE;
			}
			if (isReturn(c))
				first = TRUE;
			if (ignored && isReturn(c)) {
				ignored = FALSE;
			} else if (!ignored) {
				if (isReturn(c)) {
					comment = FALSE;
				} else if (first) {
					if (isdigit(c)) {
						ungetc(c, scriptFP);
						c = OP_ADD;
					}
					first = FALSE;
				}
				return (c);
			}
		}
		/* Finally, paint the screen if I wasn't doing so before */
		if (was_invisible) {
			int	editcols[3];
			DATA *last = EndOfData();
			ShowStatus(last, FALSE);
			ShowRange(top_data, last);
			ShowValue(last, editcols, FALSE);
			first = TRUE;
			return EQUALS;	/* flush out the last line */
		}
	}

	if (first) {
		if (scriptNUM == 1)
			scriptCHG = FALSE;
		first = FALSE;
	}
	return EOS;
}

/*
 * Read a character from an input-script, if it is available.  Otherwise, read
 * directly from the terminal.
 */
static
int	GetC(void)
{
	register int c;

	if ((c = GetScript()) == EOS) {
		show_error = FALSE;
		refresh();
		c = getch();
#if	SYS_MSDOS || defined(linux)	/* actually PD Curses */
		switch (c) {
		case PADSLASH:	c = OP_DIV;	break;
		case PADSTAR:	c = OP_MUL;	break;
		case PADPLUS:	c = OP_ADD;	break;
		case PADMINUS:	c = OP_SUB;	break;
		case PADENTER:	c = '\n';	break;
		}
#endif
	}
	return (c);
}

/*
 * Given a string, offset into it and insert-position, delete the character at that offset,
 * both from the string and screen. Return the resulting position.
 */
static
int	DeleteChar (char *buffer, int offset, int pos, int limit)
{
	int	y, x, col;
	register char	*t;

	/* delete from the actual buffer */
	for (t = buffer+offset; (t[0] = t[1]) != EOS; t++)
		;

	if (isVisible()) {	/* update the display */
		getyx(stdscr, y, x);	/* get current insert-position */
		col = x - pos + offset;	/* assume pos < len, offset < len */
		move(y, col);
		delch();
		if (limit > 0 && strlen(buffer) < limit) {
			move(y, col - offset);
			insch(' ');
			x++;
		}
		if (pos > offset)
			x--;
		move(y, x);
	}
	if (pos > offset)
		pos--;
	return pos;
}

/*
 * Insert a character into the given string, returning the updated insert
 * position.  If the "limit" parameter is nonzero, we keep the buffer
 * right-justified to that limit.
 */
static
int	InsertChar (char *buffer, int chr, int pos, int limit)
{
	int	y, x;
	int	len = strlen(buffer);
	register char	*t;

	/* perform the actual insertion into the buffer */
	for (t = buffer + len; ; t--) {
		t[1] = t[0];
		if (t == buffer+pos)
			break;
	}
	t[0] = chr;

	if (isVisible()) {	/* update the display on the screen */
		getyx(stdscr, y, x);	/* get current insert-position */
		if (x < COLS-1) {
			if (limit > 0) {
				x--;
				move(y, x - pos);
				delch();
				move(y, x);
			}
			insch((chtype)chr);
			move(y,x+1);
		}
	}
	return pos+1;
}

/*
 * Delete from the buffer the character to the left of the given col-position.
 */
static
int	doDeleteChar (char *buffer, int col, int limit)
{
	if (col > 0) {
		col = DeleteChar(buffer, col-1, col, limit);
	} else {
		Alarm();
	}
	return col;
}

/*
 * Move left within the given buffer.
 */
static
int	doMoveLeft (int col)
{
	if (col > 0) {
		register int	y, x;
		getyx(stdscr, y, x);
		move(y, x-1);
		col--;
	} else
		Alarm();
	return col;
}

/*
 * Move right within the given buffer.
 */
static
int	doMoveRight (char *buffer, int col)
{
	if (col < strlen(buffer)) {
		register int	y, x;
		getyx(stdscr, y, x);
		move(y, x+1);
		col++;
	} else
		Alarm();
	return col;
}

/*
 * Returns the index of the decimal-point in the given buffer (or -1 if not
 * found).
 */
static
int	DecimalPoint (char *buffer)
{
	register char *dot = strchr(buffer, PERIOD);
	if (dot != 0)
		return (int)(dot-buffer);
	return -1;
}

/*
 * Return the sequence-pointer of the left-parenthesis enclosing the given
 * operand-set at 'np'.
 */
static
DATA *	Balance (DATA *np, int level)
{
	int	target = level;
	while (np->prev != 0) {
		if (np->cmd == R_PAREN)		level++;
		else if (np->psh)		level--;
		if (level <= target)		break;	/* unbalanced */
		np = np->prev;
	}
	return (level == 0) ? np : 0;
}

/*
 * Edit an arbitrary buffer
 */
static
void	EditBuffer (char *buffer, int length)
{
	int	end, chr;
	int	col  = strlen(buffer);
	int	done = FALSE;

	end = COLS - CurrentCol() - 1;
	end = min(end, length);
	if (isVisible()) {
		(void) printw("%.*s", end, buffer);
		clrtoeol();
	}

	while (!done) {
		chr = GetC();
		if (isReturn(chr)) {
			done = TRUE;
		} else if (isAscii(chr) && (isprint(chr) || isspace(chr))) {
			if (strlen(buffer) < end-1)
				col = InsertChar(buffer, chr, col, 0);
			else
				Alarm();
		} else if (isDelete(chr)) {
			col = doDeleteChar(buffer, col, 0);
		} else if (isMoveLeft(chr)) {
			col = doMoveLeft(col);
		} else if (isMoveRight(chr)) {
			col = doMoveRight(buffer, col);
		} else {
			Alarm();
		}
	}
}

/*
 * Edit the comment-field
 */
static
void	EditComment (DATA *np)
{
	char	buffer[BUFSIZ];
	int	row,
		editcols[3];

	(void)strcpy(buffer, np->txt != 0 ? np->txt : "");

	ShowStatus(np, -1);
	row = ShowValue(np, editcols, TRUE) + 1;
	if (isVisible()) {
		move(row, editcols[1]);
		addstr(" # ");
	}
	EditBuffer(buffer, sizeof(buffer));
	TrimString(buffer);

	if (*buffer != EOS || (np->txt != 0)) {
		if (np->txt != 0) {
			if (!strcmp(buffer, np->txt))
				return;	/* no change needed */
			free(np->txt);
		}
		np->txt = (*buffer != EOS) ? AllocString(buffer) : 0;
	}
}

/*
 * Returns true if the given entry has editable data
 */
static
int	HasData(DATA *np)
{
	return !LastData(np) || (np->val != 0.0);
}

/*
 * Read a new number, until the operator for the next number is encountered.
 * Inputs:
 *	np	= line entry at which to prompt/read data
 *	edit	= true iff we re-edit prior contents of this line
 * Outputs:
 *	*len_	= -2 if right parenthesis found,
 *		= -1 if left parenthesis found,
 *		=  0 if no number found (usually to switch operators)
 *		= +n if value found, i.e., its length.
 *	*val_	= the decoded number.
 * Returns:
 *	The terminating character (e.g., an operator or command).
 */
static
int	EditValue (DATA *np, int *len_, Value *val_, int edit)
{
	int	c,
		row,
		col,			/* current insert-position */
		editcols[3],
		done = FALSE,
		was_visible = isVisible(),
		nesting;		/* if we find left parenthesis rather than number */
	char	buffer[MAXBFR];		/* current input value */

	static	char	old_digit = EOS;	/* nonzero iff we have pending digit */

	if (np->cmd == R_PAREN)
		edit = FALSE;

	ShowStatus(np, FALSE);
	row = ShowValue(np, editcols, FALSE) + 1;

	if (isVisible()) {
		move(row, editcols[0]);
		BeginHigh();
	}

	if (edit) {
		if (np->psh) {
			buffer[0] = L_PAREN;
			buffer[1] = EOS;
		} else {
			register int len, dot;
			register char *s;

			(void) sprintf (buffer, "%0*.0f", len_frac, np->val);
			len = strlen(buffer);
			s = buffer + len;
			s[1] = EOS;
			for (c = 0; c < len_frac; c++, s--)
				s[0] = s[-1];
			dot = len - len_frac;
			len++;
			buffer[dot] = PERIOD;
		}
		if (isVisible()) {
			move(row, (int)(editcols[0] + val_width - strlen(buffer)));
			addstr (buffer);
		}
	} else {
		buffer[0] = EOS;
	}

	if (isVisible())
		move(row, editcols[0] + val_width);
	col = strlen(buffer);
	nesting = (*buffer == L_PAREN);
	c = EOS;

	while (!done) {
		if (old_digit) {
			c = old_digit;
			old_digit = EOS;
		} else {
			c = GetC();
		}

		/*
		 * If the current operator is a right parenthesis, we must have an operator
		 * following, with no data intervening:
		 */
		if (np->cmd == R_PAREN) {
			if (isDigit(c)
			 || (c == PERIOD)
			 || (c == L_PAREN)) {
				Alarm ();
			} else {
				*len_ = -2;
				done  = TRUE;
			}
		}
		/*
		 * Move left/right within the buffer to adjust the insertion
		 * position
		 */
		else if (isMoveLeft(c))
			col = doMoveLeft(col);
		else if (isMoveRight(c))
			col = doMoveRight(buffer, col);
		/*
		 * Backspace deletes the last character entered:
		 */
		else if (isDelete(c)) {
			col = doDeleteChar(buffer, col, val_width);
			if (*buffer == EOS)
				nesting = FALSE;
		}
		/*
		 * A left parenthesis may be used only as the first (and only)
		 * character of the operand.
		 */
		else if (c == L_PAREN) {
			if (*buffer != EOS) {
				Alarm ();
			} else {
				col = InsertChar(buffer, c, col, val_width);
				nesting = TRUE;
			}
		}
		/*
		 * If we have received a left parenthesis, and do not delete
		 * it, the next character begins a new operand-line:
		 */
		else if (nesting) {
			if (UnaryConflict(np, c))
				Alarm();
			else {
				if (isDigit(c) || c == PERIOD) {
					old_digit = c;
					c = OP_ADD;
				}
				*len_ = -1;
				done  = TRUE;
			}
		}
		/*
		 * Otherwise, we assume we have a normal value which we are
		 * decoding:
		 */
		else if (isDigit(c))
			col = InsertChar(buffer, c, col, val_width);
		/*
		 * Decimal point can be entered once for each number. If we
		 * get another, simply move it to the new position.
		 */
		else if (c == PERIOD) {
			register int dot;
			if ((dot = DecimalPoint(buffer)) >= 0)
				col = DeleteChar(buffer, dot, col, val_width);
			col = InsertChar(buffer, c, col, val_width);
		}
		/*
		 * Otherwise, we assume a new operator-character for the
		 * next command, flushing out the current command.  Decode
		 * the number (if any) which we have read:
		 */
		else if (c != COMMA) {
			if (*buffer != EOS) {
				register int len = strlen(buffer);
				register int dot;
				Value cents;

				if ((dot = DecimalPoint(buffer)) < 0)
					buffer[dot = len++] = PERIOD;
				while ((len-dot) <= len_frac)
					buffer[len++] = '0';
				len = dot + 1 + len_frac; /* truncate */
				buffer[len] = EOS;
				(void) sscanf (&buffer[dot+1], "%lf", &cents);
				if (dot) {
					buffer[dot] = EOS;
					(void) sscanf (buffer, "%lf", val_);
					buffer[dot] = PERIOD;
					*val_ *= val_frac;
				} else
					*val_ = 0.0;
				*val_ += cents;
			} else {
				*val_ = np->val;
			}
			*len_ = (*buffer == L_PAREN) ? 0 : strlen(buffer);
			done  = TRUE;
		}
	}
	if (was_visible)
		EndOfHigh();
	return (c);
}

/*
 * Return true if (given the length returned by 'EditValue()', and the
 * state of the data-list) we don't display a value.
 */
static
int	NoValue (int len)
{
	return ((len == 0 || len == -2) && (all_data->next->next != 0));
}

/*
 * Compute one stage of the running total.  To support parentheses, we use two
 * arguments: 'old' is the operand which contains the left parenthesis.
 */
static
int	Calculate (DATA *np, DATA *old)
{
	Bool same;
	Value before = np->sum;

	np->sum = (old->prev) ? old->prev->sum : 0.0;
	switch (old->cmd) {
	case OP_ADD:
		np->sum += np->val;
		break;
	case OP_SUB:
		np->sum -= np->val;
		break;
	case OP_MUL:
		np->sum *= (np->val / val_frac);
		np->sum = Floor(np->sum);
		break;
	case OP_DIV:
		if (np->val == 0.0) {
			if (np->sum > 0.0)	np->sum = big_long;
			else if (np->sum < 0.0)	np->sum = -big_long;
		} else {
			np->sum /= (np->val / val_frac);
			np->sum = Floor(np->sum);
		}
		break;
	case OP_INT:
		np->aux = Ceiling(np->sum * np->val / (interval * 100. * val_frac));
		np->sum += np->aux;
		break;
	case OP_TAX:
		np->aux = Ceiling(np->sum * np->val / (100. * val_frac));
		np->sum += np->aux;
		break;
	}

	same = (before == np->sum)
		&& (before < big_long)
		&& (before > -big_long);

	if (isVisible() && !same)
		scriptCHG = TRUE;
	return (same);
}

/*
 * Given a pointer 'np' into the operand list, and (possibly) new 'cmd' and
 * 'val' components, propagate the computation to the end of the vector,
 * showing the result on the screen.
 */
static
void	Recompute (DATA *base)
{
	register DATA *np = base;
	register DATA *op;
	int	level = LevelOf(np);
	Bool	same;

	while (np != 0) {
		if (np->psh) {
			np->sum = 0.0;
			level++;
		} else {
			if (np->cmd == R_PAREN) {
				level--;
				np->val = np->prev->sum;
				op = Balance(np, level);
			} else {
				op = np;
			}
			same = Calculate(np, op);
			if ((level == 0) && same)
				break;
		}
		np = np->next;
	}

	ShowRange(base, np ? np->next : np);
}

/*
 * "Open" a new entry for editing.  If 'after' is set, we open the entry
 * after the current 'base'.  This is the normal mode of operation, and is
 * consistent with 'x'-command.
 */
static
DATA *	OpenLine (DATA *base, int after, int *repeated, int *edit)
{
	DATA	*save_top = top_data;
	DATA	*op = after ? base : base->prev;
	DATA	*np;
	int	chr,
		this_row,
		done = FALSE,
		nested;

	np = AllocData(op);
	nested   = LevelOf(np);
	this_row = CountFromTop(np);

	/* Adjust 'top_data' if we have to scroll a little */
	if (this_row <= 1) {
		while (this_row++ <= 1) {
			if (top_data->prev == all_data)
				break;
			top_data = top_data->prev;
		}
	} else {
		this_row -= (full - 2);
		while (this_row-- > 0)
			top_data = top_data->next;
	}

	/* (Re)display the screen with the opened line */
	if (top_data == save_top)
		ShowFrom(np->next);
	else
		ShowFrom(top_data);
	ShowStatus(np, TRUE + nested);
	clrtoeol();
	*edit = FALSE;	/* assume we'll get some new data */

	while (!done) {
		switch (chr = GetC()) {
		case OP_INT:			/* sC: open to interest */
		case OP_TAX:			/* sC: open to sales tax */
						/* patch: provide defaults */
		case OP_ADD:			/* sC: open to add */
		case OP_SUB:			/* sC: open to subtract */
		case R_PAREN:			/* sC: open closing brace */
			setval (np, chr, 0.0, FALSE);
			done++;
			break;
		case L_PAREN:
			setval (np, OP_ADD, 0.0, TRUE);
			*edit = TRUE;		/* force this to display */
			done++;
			break;
		case OP_MUL:			/* sC: open to multiply */
		case OP_DIV:			/* sC: open to divide */
			setval (np, chr, val_frac, FALSE);
			done++;
			break;
		case 'a': case 's':
		case 'm': case 'd':
		case 'i': case 't':
			chr = isRepeats(chr);
			setval (np, chr, LastVAL(np,chr), FALSE);
			done++;
			*repeated = TRUE;
			break;
		case 'q': case 'Q':
		case 'o': case 'O':
		case 'x': case 'X':
		case 'u': case 'U':
			(void) FreeData(np, FALSE);
			if (top_data != save_top) {
				top_data = save_top;
				ShowFrom(top_data);
			}
			np = base;
			done++;
			*edit = TRUE;	/* go back to the original */
			break;
		default:
			Alarm();
		}
	}
	return (np);
}

/*
 * Perform half/full-screen scrolling:
 */
static
DATA *	ScrollBy (DATA *np, int amount)
{
	int	last_seq = CountAllData();
	int	this_seq = CountData(np);
	int	next_seq = this_seq;
	int	top = CountData(top_data);

	if (amount > 0) {
		if ((top + amount) < last_seq) {
			top += amount;
			next_seq = top;
		} else
			next_seq = last_seq;
	} else {
		if (this_seq > top)
			next_seq = top;
		else {
			next_seq = top+amount;
			next_seq = max(next_seq, 1);
			top = next_seq;
		}
	}
	ShowFrom(top_data = FindData(top));
	return FindData(next_seq);
}

/*
 * Compute a one-line movement of the cursor. The interaction between the
 * 'flush' and 'amount' arguments compensates for other adjustments to the
 * current data pointer in the calling functions.
 */
static
DATA *	JumpBy (DATA *np, int amount)
{
	int	this_seq = CountData(np);
	int	next_seq = this_seq + amount,
		last_seq = CountAllData();
	Bool	end_flag = TRUE,
		un_moved = FALSE;

	if (next_seq < 1) {
		next_seq = 1;
	} else if (next_seq > last_seq) {
		next_seq = last_seq;
	} else {
		end_flag = FALSE;
	}

	if (next_seq != this_seq) {
		np = FindData(next_seq);
	} else if (end_flag) {
		un_moved = TRUE;
	}

	/* Figure out if we have to adjust the top_data variable.
	 * If so, we've got to redisplay the screen.
	 */
	if (!un_moved) {
		int	top = CountData(top_data);
		Bool	adjust = TRUE;
		if (next_seq < top)
			top_data = np;
		else if (next_seq >= top+full)
			top_data = FindData(next_seq - full + 1);
		else
			adjust = FALSE;
		if (adjust)
			ShowFrom(top_data);
	}
	return np;
}

/*
 * Jump to a specified entry, by number
 */
static
DATA *	JumpTo (DATA *np, int seq)
{
	return JumpBy(np, seq-CountData(np));
}

/*
 * Prompt/process a :-command
 */
static
DATA *	ColonCommand (DATA *np)
{
	DATA	*save_top = top_data;
	DATA	*prior_np = np;
	char	buffer[BUFSIZ];
	char	*reply;
	static	char	*last_write = "";

	if (CountFromTop(np) >= full-1) {
		top_data = top_data->next;
		ShowFrom(top_data);
	}
	move(LINES-1,0);
	addch(COLON);
	addch(' ');
	*buffer = EOS;
	EditBuffer(buffer, sizeof(buffer));
	TrimString(buffer);
	reply = buffer;
	while (isspace(*reply))
		reply++;

	if (*reply != EOS) {
		if (isdigit(*reply)) {
			char *dst;
			int seq = (int)strtol(reply, &dst, 0);
			np = JumpTo(np, seq);
		} else {
			char *param = reply+1;
			while (isspace(*param))
				param++;
			switch (*reply) {
			case '$': /* FALLTHRU */
			case '%':
				np = JumpTo(np, CountAllData());
				break;
			case 'e':
				np = all_data->next;
				while (np->next != 0)
					np = FreeData(np, TRUE);
				save_top = top_data;
				setval(np, OP_ADD, 0.0, FALSE);
				Recompute(np);
				/* FALLTHRU */
			case 'r':
				if (Ok2Read(param)) {
					static	char	*argv[2];
					argv[0] = AllocString(param);
					scriptv = argv;
				}
				break;
			case 'w':
				if (*param == EOS)
					param = last_write;
				if (*param == EOS)
					param = top_output;
				if (Ok2Write(param)) {
					last_write = AllocString(param);
					PutScript(param);
				}
				break;
			default:
				Alarm();
			}
		}
	}

	if (top_data != save_top
	 && prior_np == np) {
		top_data = save_top;
		ShowFrom(top_data);
	}
	return np;
}

/*
 * Do simple screen movement. Note that some movement-commands may be
 * printing characters, so (if in edit-mode) we may have already intercepted
 * these as text.
 */
static
int	ScreenMovement (DATA **pp, int chr)
{
	DATA	*np = *pp;
	int	ok = TRUE;

	if (chr == COLON) {
		np = ColonCommand(np);
	} else if (chr == 'z') {
		chr = GetC();
		if (isReturn(chr)) {
			top_data = np;
		} else {
			int	top = CountData(top_data);
			int	seq = CountData(np);
			if (chr == '-') { /* use current entry as end */
				top = seq - full + 1;
			} else {
				top = seq - (CountData(ScreenBottom()) - top)/2;
			}
			top = max(top, 1);
			top_data = FindData(top);
		}
		ShowFrom(top_data);
	} else if (chr == 'H') {	/* C: move to first entry on screen */
		np = top_data;
	} else if (chr == 'L') {	/* C: move to last entry on screen */
		np = ScreenBottom();
	} else if (isMoveDown(chr)) {	/* C: move down 1 line */
		np = JumpBy(np, 1);
	} else if (isMoveUp(chr)) {	/* C: move up 1 line */
		np = JumpBy(np, -1);
	} else if (chr == CTL('D')) {	/* C: scroll forward 1/2 screen */
		np = ScrollBy(np, half);
	} else if (chr == CTL('U')) {	/* C: scroll backward 1/2 screen */
		np = ScrollBy(np, -half);
	} else if (isPageDown(chr)) {	/* C: scroll forward one screen */
		np = ScrollBy(np, full);
	} else if (isPageUp(chr)) {	/* C: scroll backward one screen */
		np = ScrollBy(np, -full);
	} else {
		ok = FALSE;
	}

	*pp = np;
	return ok;
}

/*
 * Display the help file.
 * We store the help-text as a special case of the data list to permit use
 * of the scrolling code.
 */
static
void	ShowHelp(void)
{
	FILE	*fp;
	DATA	*save_data = all_data;
	DATA	*save_top  = top_data;
	DATA	*np;
	int	chr,
		end;
	int	done = FALSE;
	char	buffer[BUFSIZ];

	if ((all_data = all_help) == 0) {

		np = AllocData((DATA *)0);	/* header line not shown */
		if ((fp = fopen(helpfile, "r")) != 0) {
			while (fgets(buffer, sizeof(buffer), fp) != 0) {
				np = AllocData(np);
				np->txt = AllocString(buffer);
			}
			(void) fclose(fp);
		} else {
			np = AllocData(np);
			np->txt = "Could not find help-file.  Press 'q' to exit.";
		}
	}

	np = top_data = all_data->next;
	end = CountAllData();
	ShowFrom(all_data);

	while (!done) {
		move(0, 0);
		BeginBold();
		clrtoeol();
		(void) sprintf(buffer, "line %d of %d", CountData(np), end);
		move (0, (int)(COLS - (strlen(buffer)+1)));
		addstr(buffer);
		move (0, 0);
		(void) printw("%s -- ", copyrite);
		EndOfBold();
		move(CountFromTop(np)+1, 0);
		chr = GetC();
		if (chr == 'q' || chr == 'Q') {
			done = TRUE;
		} else if (!ScreenMovement(&np, chr)) {
			Alarm();
		}
	}

	all_help = all_data;
	all_data = save_data;
	top_data = save_top;
	ShowFrom(top_data);
}

#ifndef VMS
# ifdef	unix
#  define isSlash(c) ((c) == '/')
# else
#  define isSlash(c) ((c) == '/' || (c) == '\\')
# endif
static
int	AbsolutePath (char *path)
{
#if	!defined(unix) && !defined(vms)	/* assume MSDOS */
	if (isalpha(*path) && path[1] == ':')
		path += 2;
#endif
	return isSlash(*path);
}

static
char *	PathLeaf (char *path)
{
	register int n;
	for (n = strlen(path); n > 0; n--)
		if (isSlash(path[n-1]))
			break;
	return path + n;
}
#endif

/*
 * Find the help-file. On UNIX and MSDOS, we look for the file in the
 * same directory as that in which this program is stored, located by
 * searching the PATH environment variable.
 */
static
void	FindHelp (char *program)
{
#ifdef ADD_HELPFILE
	helpfile = ADD_HELPFILE;
#else
	char	temp[BUFSIZ];
	register char *s = strcpy(temp, program);
# if SYS_VMS
	for (s += strlen(temp); s != temp; s--)
		if (s[-1] == ']' || s[-1] == ':')
			break;
# else	/* assume UNIX or MSDOS */
	if (AbsolutePath(temp)) {
		s = PathLeaf(temp);
	} else {
		char	*path = getenv("PATH");
		int	j = 0, k, l;
		if (path == 0)
			path = "";
		while (path[j] != EOS) {
			for (k = j; path[k] != EOS && path[k] != PATHSEP; k++)
				temp[k-j] = path[k];
			if ((l = k - j) != 0)
				temp[l++] = '/';
			s = strcpy(temp+l, program);
			if (access(temp, 5) == 0) {
				temp[l] = EOS;
				break;
			}
			j = (path[k] != EOS) ? k+1 : k;
		}
		if (path[j] == EOS) {
			s = temp;
			*s++ = '.';
			*s++ = '/';
			s = PathLeaf(strcpy(s, program));
		}
	}
# endif	/* VMS/UNIX/MSDOS */
	(void) strcpy(s, "add.hlp");
	helpfile = AllocString(temp);
#endif	/* ADD_HELPFILE */
}

/*
 * Main program loop: given 'old' operator (applies to current entry), read the
 * value 'val', delimited by the next operator 'chr'.
 */
static
int	Loop(void)
{
	DATA *np = EndOfData();
	Value	val;
	int	test_c,
		chr,
		len,
		opened,
		repeated,	/* if true, 'Loop' assumes editable value */
		edit	= FALSE;

	for (;;) {
		chr = EditValue(np, &len, &val, edit);
		switch (chr) {
		case '\t':	chr = EQUALS;	break;
		case '\r':
		case '\n':	chr = 'j';	break;
		case ' ':	chr = DefaultOp(np);
		}

		if (chr == 'X') { /* C: delete current entry, move up */
			if (NoValue(len)) {
				np = FreeData(np, TRUE);
				np = JumpBy(np, -1);
				edit = HasData(np);
			} else {
				edit = FALSE;
			}
		} else if (chr == 'x') { /* C: delete current entry, move down */
			if (NoValue(len)) {
				np = FreeData(np, TRUE);
				edit = HasData(np);
			} else {
				edit = FALSE;
			}
		} else if (chr == 'u') { /* C: undo last 'x' command */
			if (HasData(np))
				edit = TRUE;
			else
				Alarm();
		} else if ((test_c = isToggles(chr)) != EOS) {
			/* C: toggle current operator */
			chr = test_c;
			if (UnaryConflict(np, chr)) {
				Alarm ();
			} else {
				if (len == 0)
					val = np->val;
				else
					edit = TRUE;
				setval (np, chr, val, np->psh);
			}
		} else {
			if (len != 0) {
				setval (np, np->cmd, val, (len == -1));
				if (LastData(np)
				 && (isCommand(chr) || isRepeats(chr))) {
					(void)AllocData(np);
					np->next->cmd = DefaultOp(np);
				}
				Recompute (np);
			} else {
				(void)ShowValue(np, (int *)0, FALSE);
			}

			repeated = FALSE;
			opened = FALSE;

			if (chr == '?') {		/* C: display help file */
				ShowHelp();
			} else if (chr == '#') {	/* C: edit comment */
				EditComment(np);
			} else if (chr == 'Q') {	/* C: quit w/o changes */
				return (FALSE);
			} else if (chr == 'q') {	/* C: quit */
				return (TRUE);
			} else if (ScreenMovement(&np, chr)) {
				;
			} else if (chr == 'O'		/* C: open before */
			   ||      chr == 'o') {	/* C: open after */
				opened = TRUE;
				np = OpenLine(np, (chr == 'o'),
					&repeated, &edit);
				if (repeated)
					chr = np->cmd;
			} else {
				if ((test_c = isRepeats(chr)) != EOS) {
					chr = test_c;
					repeated = TRUE;
				}
				if (isCommand(chr)) {
					np = JumpBy(np, 1);
					np->cmd = chr;
				} else if (chr == EQUALS) {
					Recompute(np);
				} else {
					Alarm();
				}
			}
			if (repeated) {
				edit = TRUE;
				setval (np, chr, LastVAL(np,chr), FALSE);
			} else if (!opened) {
				edit = HasData(np);
			}
		}
	}
}

static
void	usage(void)
{
	static	const	char *tbl[] = {
	 "Usage: add [options] [scripts]"
	,""
	,"Options:"
	,"  -p num       specify precision (default=2)"
	,"  -i interval  specify compounding-interval (default=12)"
	,"  -o script    specify output-script name (default is the first"
	,"               input-script name)"
	,""
	,"Description:"
	,"  Script-based adding machine that allows you to edit the operations"
	,"  and data."
	};
	int j;

	for (j = 0; j < SIZEOF(tbl); j++)
		fprintf(stderr, "%s\n", tbl[j]);
	exit(EXIT_FAILURE);
}

#if !HAVE_GETOPT
int	optind;
int	optchr;
char *	optarg;

int	getopt(int argc, char **argv, char *opts)
{
	if (++optind < argc
	 && (optarg = argv[optind]) != 0
	 && *optarg == '-') {
		if (*(++optarg) != ':'
		 && (optchr = *optarg) != EOS
		 && strchr(opts, *(optarg++)) != 0)
			return optchr;
	}
	return EOF;
}
#endif

int	main (int argc, char **argv)
{
	long	k;
	int	j,
		max_digits,		/* maximum length of a number */
		changed;
	char	tmp;
	Bool	o_option = FALSE;

	(void) signal(SIGFPE, SIG_IGN);
	len_frac = 2;
	interval= 12;

	/*
	 * Compute the maximum number of digits to display:
	 *
	 *	big_long - the maximum positive value that we can stuff into
	 *		   a 'long'. Assume symmetry, i.e., that we can use
	 *		   the same negative magnitude.
	 *	val_width - the maximum length of a formatted number, counting
	 *		   sign, decimal point and commas between groups
	 *		   of digits.
	 */
	big_long = k = 1;
	while ((k = (k << 1) + 1) > big_long)
		big_long = k;

	for (k = big_long, max_digits = 0; k >= 10; k /= 10)
		max_digits++;

	FindHelp(argv[0]);

	while ((j = getopt(argc, argv, "p:i:o:")) != EOF)
		switch(j) {
		case 'p':
			if ((sscanf (optarg, "%d%c", &len_frac, &tmp) != 1)
			 || (len_frac <= 0 || len_frac > max_digits-2))
				usage();
			break;
		case 'i':
			if ((sscanf (optarg, "%d%c", &interval, &tmp) != 1)
			 || (interval <= 0 || interval > 100))
				usage();
			break;
		case 'o':
			o_option = TRUE;
			top_output = optarg;
			break;
		default:
			usage();
		}

	for (j = 0, val_frac = 1.0; j < len_frac; j++)
		val_frac *= 10.0;

	val_width = 1 + ((max_digits - len_frac) + 2)/3 + max_digits + 1;

	/*
	 * Allocate some dummy data so we can propagate results from it.
	 */
	all_data = 0;
	for (j = 0; j < 2; j++) {
		setval (AllocData((DATA *)0), OP_ADD, 0.0, FALSE);
	}
	top_data = all_data->next;

	/*
	 * If we have input scripts, save a pointer to the list:
	 */
	if (optind < argc) {
		if (top_output == 0
		 && !Fexists(argv[optind]))
			top_output = argv[optind++];
		scriptv = argv + optind;
		for (j = 0; scriptv[j] != 0; j++) {
			(void)Ok2Read(scriptv[j]);
		}
	} else {
		scriptv = argv + argc;	/* points to a null-pointer */
	}

	/*
	 * Verify if we will be able to write an output file:
	 */
	if (optind < argc && !top_output)
		top_output = argv[argc-1];

	if (top_output == 0)
		top_output = "";
	if (*top_output != EOS)
		(void)Ok2Write(top_output);

	/*
	 * Setup and run the interactive portion of the program.
	 */
	if (initscr () == 0)	/* should return a "WINDOW *" */
		exit(EXIT_FAILURE);
#if HAVE_KEYPAD
	keypad(stdscr, TRUE);
#endif
#if defined(COLOR_BLUE) && defined(COLOR_WHITE) && defined(COLOR_PAIR)
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
	typeahead(-1);	/* disable typeahead */
#endif
#if SYS_MSDOS && defined(F_GRAY) && defined(B_BLUE)
	wattrset(stdscr, F_GRAY | B_BLUE); /* patch for old PD-Curses */
#endif
	raw(); nonl(); noecho();
	full    = LINES-1;
	half    = (full+1)/2;
	cursed  = TRUE;
	changed = Loop();
	endwin ();
	cursed  = FALSE;	/* flag showing that curses is off */

	/*
	 * If one or more scripts were given as input, and a '-o' argument
	 * was given, overwrite the last one with the results.
	 */
	if (*top_output && changed && scriptCHG)
		PutScript(top_output);

#if NO_LEAKS
	free(helpfile);
	while (FreeData(all_data, FALSE) != 0)
		/*EMPTY*/;
#if HAVE_DBMALLOC_H
	free(-1);		/* FIXME: force linux+dbmalloc to report */
#endif
#endif

	return (EXIT_SUCCESS);
}
