/******************************************************************************
 * Copyright 1995-2022,2024 by Thomas E. Dickey                               *
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

static const char copyrite[] = "Copyright 1994-2022,2024 by Thomas E. Dickey";

/*
 * Title:	add.c
 * Author:	T.E.Dickey
 * Created:	05 May 1986
 * Modified:	(see CHANGES)
 *
 * Function:	This is a simple adding machine that uses curses to display
 *		a column of values, operators and results.  The user can
 *		move up and down in the column, modifying the values and
 *		operators.
 *
 * $Id: add.c,v 1.71 2024/09/16 21:21:41 tom Exp $
 */

#include <add.h>
#include <screen.h>

#define	SALLOC(s) (s *)malloc(sizeof(s))

#define	SSTACK	struct	SStack
SSTACK {
    SSTACK *next;
    FILE *sfp;
    char **sscripts;
};

#define IsEmpty(s) ((s) == NULL || *(s) == '\0')

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif

static GCC_NORETURN void usage(void);
static GCC_NORETURN void failed(const char *);

static void Recompute(DATA *);
static void ShowFrom(DATA *);

/*
 * Common data
 */
static char sep_radix = '.';
static char sep_group = ',';
static const char *top_output;
static DATA *all_data;		/* => beginning of data */
static DATA *top_data;		/* => beginning of current screen */

static Value val_frac;		/* # of units in 'len_frac' (e.g., 100.0) */
static long big_long;		/* largest signed 'long' value */
static int interval;		/* compounding interval-divisor */
static int max_width;		/* maximum width of formatted number */
static int use_width;		/* working width of formatted number */
static int len_frac;		/* nominal number of digits after period */
static Bool show_error;		/* suppress normal reporting until GetC() */
static Bool show_scripts;	/* force script to be visible, for testing */

/*
 * Input-script control:
 */
static SSTACK *sstack;		/* script-stack */
static FILE *scriptFP;		/* current script file-pointer */
static char **scriptv;		/* pointer to list of input-scripts */
static Bool scriptCHG;		/* set true if there's a change after scripts */
static Bool scriptNUM;		/* set true to 0..n for script number */
static Bool had_error;		/* errors in script hint at the wrong file */
static int scriptLine;		/* line number in current script */
static int scriptErrs;		/* count successive errors */

/*
 * Help-screen
 */
static DATA *all_help;
static char *helpfile;

#define CMD(op,l,u,f,s) {op, l, u, f, s}

/*
 * Check to see if the given character is a legal 'add' operator (excluding
 * editing/scrolling):
 */
/* *INDENT-OFF* */
static struct {
    char command;
    char repeats;
    char toggles;
    Bool isunary;
    const char *explain;
} Commands[] = {
    CMD(OP_ADD,  'a',	'A',	TRUE,	"add"),
    CMD(OP_SUB,  's',	'S',	TRUE,	"subtract"),
    CMD(OP_NEG,  'n',	'N',	FALSE,	"negate"),
    CMD(OP_MUL,  'm',	'M',	FALSE,	"multiply"),
    CMD(OP_DIV,  'd',	'D',	FALSE,	"divide"),
    CMD(OP_INT,  'i',	'I',	FALSE,	"interest"),
    CMD(OP_TAX,  't',	'T',	FALSE,	"tax"),
    CMD(L_PAREN, EOS,	EOS,    TRUE,	"begin group"),
    CMD(R_PAREN, EOS,	EOS,    FALSE,	"end group")
};
/* *INDENT-ON* */

static void
failed(const char *msg)
{
    screen_finish();
    perror(msg);
    exit(EXIT_FAILURE);
}

static void
HadError(void)
{
    had_error = TRUE;
    screen_alarm();
}

/*
 * Normally we don't show the results of replaying a script. This makes
 * loading scripts much faster.
 */
static int
isVisible(void)
{
    return show_scripts || (scriptFP == 0);
}

/*
 * Lookup a character to see if it is a legal operator, returning nonnull
 * in that case.
 */
#define	LOOKUP(func,lookup,result) \
	static	int	func(int c) { \
			unsigned j; \
			for (j = 0; j < SIZEOF(Commands); j++) \
				if (Commands[j].lookup == c) \
					return result; \
			return 0; \
			}
/* *INDENT-OFF* */
LOOKUP(isCommand, command, c)
LOOKUP(isRepeats, repeats, Commands[j].command)
LOOKUP(isToggles, toggles, Commands[j].command)
LOOKUP(isUnary, command, Commands[j].isunary)
/* *INDENT-ON* */

/*
 * Find the end of the DATA list
 */
static
DATA *
EndOfData(void)
{
    DATA *np;
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
static int
FirstData(DATA * np)
{
    return (np->prev == 0 || np->prev == all_data);
}

/*
 * Tests for special case of operators that cannot appear in a unary context.
 */
static int
UnaryConflict(DATA * np, int chr)
{
    if (isCommand(chr))
	return (!isUnary(chr) && (FirstData(np) || np->prev->psh));
    return FALSE;
}

/*
 * Trim whitespace from the end of a string
 */
static void
TrimString(char *src)
{
    char *end = src + strlen(src);

    while (end-- != src) {
	if (isspace(UCH(*end)))
	    *end = EOS;
	else
	    break;
    }
}

/*
 * Allocate a string, trimming whitespace from the end for consistency.
 */
static char *
AllocString(const char *src)
{
    char *result = strdup(src);
    size_t len = strlen(result);
    while (len != 0 && isspace((unsigned char) (result[--len])))
	result[len] = '\0';
    return result;
}

/*
 * Allocate and initialize a DATA entry
 */
static DATA *
AllocData(DATA * after)
{
    DATA *np = SALLOC(DATA);

    if (np == 0)
	failed("AllocData");

    assert(np != 0);

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
    } else {			/* append to the end of the list */
	DATA *op = EndOfData();
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
static DATA *
FreeData(DATA * np, int permanent)
{
    DATA *prev = np->prev;
    DATA *next = np->next;

    if (prev == 0) {		/* we're at the beginning of the list */
	all_data = next;
	if (next != 0)
	    next->prev = 0;
    } else {
	prev->next = next;
	if (next != 0) {
	    next->prev = prev;
	} else {		/* deleted the end-of-data */
	    next = prev;
	}
    }
    if (np->txt != 0)
	free(np->txt);
    free((char *) np);

    if (top_data == np)
	top_data = next;

    if (screen_active) {
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
static int
CountData(DATA * np)
{
    int seq = 0;
    while (np != 0 && np->prev != 0) {
	seq++;
	np = np->prev;
    }
    return seq;
}

static int
CountFromTop(DATA * np)
{
    return CountData(np) - CountData(top_data);
}

static int
CountAllData(void)
{
    return CountData(EndOfData());
}

static DATA *
FindData(int seq)
{
    DATA *np = all_data;
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
static Value
Floor(Value val)
{
    if (val > big_long) {
	val = (Value) big_long;
    } else if (val < -big_long) {
	val = (Value) (-big_long);
    } else {
	long tmp = (long) val;
	val = (Value) tmp;
    }
    return (val);
}

static Value
Ceiling(Value val)
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
static Value
LastVAL(const DATA * np, int cmd)
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
static char *
Format(char *dst, Value val)
{
    int neg = val < 0.0;
    char bfr[MAXBFR], *s = dst;

    if (neg) {
	val = -val;
	*s++ = OP_SUB;
    }

    if (val >= big_long) {
	(void) strcpy(s, " ** overflow");
    } else {
	int len;
	int j;
	size_t grp;

	(void) sprintf(bfr, "%0*.0f", len_frac, val);
	len = (int) strlen(bfr) - len_frac;
	grp = (size_t) (len % 3);
	j = 0;

	while (j < len) {
	    if (grp) {
		(void) strncpy(s, &bfr[j], grp);
		j += (int) grp;
		s += grp;
		if (j < len)
		    *s++ = sep_group;
	    }
	    grp = 3;
	}
	(void) sprintf(s, "%c%s", sep_radix, &bfr[len]);
    }
    return (dst);
}

/*
 * Convert a value to printing form, writing it on the screen:
 */
static void
putval(Value val)
{
    char bfr[MAXBFR];

    screen_printf("%*.*s", use_width, use_width, Format(bfr, val));
}

/*
 */
static void
setval(DATA * np, int cmd, Value val, int psh)
{
    np->cmd = (char) (isCommand(cmd) ? cmd : OP_ADD);
    np->val = val;
    np->psh = psh;
}

/*
 * Compute the parenthesis level of the given data entry.  This is a positive
 * number.
 */
static int
LevelOf(const DATA * target)
{
    DATA *np;
    int level = 0;

    for (np = all_data; np != 0 && np != target; np = np->next) {
	if (np->cmd == R_PAREN)
	    level--;
	if (np->psh)
	    level++;
    }
    return level;
}

/*
 * Return a pointer to the last data entry in the screen.
 */
static DATA *
ScreenBottom(void)
{
    DATA *np = top_data;
    int count = screen_full;

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
static int
ShowValue(DATA * np, int *editing, Bool comment)
{
    int row = CountFromTop(np);
    int col = 0;
    int level;

    if (editing != 0) {
	editing[0] =
	    editing[1] = 0;
    }
    if (isVisible() && row >= 0 && row < screen_full) {
	char cmd = (char) (isprint(UCH(np->cmd)) ? np->cmd : '?');

	screen_set_position(row + 1, col);
	screen_clear_endline();
	if (np->cmd != EOS) {
	    for (level = LevelOf(np); level > 0; level--)
		screen_puts(". ");
	    if (editing != 0) {
		screen_set_reverse(TRUE);
		screen_printf(" %c>>", cmd);
		screen_set_reverse(FALSE);
	    } else {
		screen_printf(" %c: ", cmd);
	    }

	    if (editing != 0) {
		*editing = screen_col();
		screen_set_reverse(TRUE);
	    }

	    if ((cmd == R_PAREN) || ((editing != 0) && !comment))
		screen_printf("%*.*s", use_width, use_width, " ");
	    else if (np->psh)
		screen_putc(L_PAREN);
	    else
		putval(np->val);

	    if (editing != 0)
		screen_set_reverse(FALSE);

	    if (!np->psh) {
		screen_puts(" ");
		if (editing != 0)
		    screen_set_bold(TRUE);
		putval(np->sum);
		if (editing != 0)
		    screen_set_bold(FALSE);
	    }
	    if (cmd == OP_INT || cmd == OP_TAX) {
		screen_puts(" ");
		putval(np->aux);
	    }

	    col = screen_col();
	    if (editing != 0)
		editing[1] = col;
	    col += 3;
	    if ((np->txt != 0) && screen_cols_left(col) > 3)
		screen_puts(" # ");
	}

	if ((np->txt != 0)
	    && screen_cols_left(col) > 0) {
	    screen_printf("%.*s", screen_cols_left(col), np->txt);
	}
	if (LastData(np) && screen_rows_left(row) > 1) {
	    screen_set_position(row + 2, 0);
	    screen_clear_bottom();
	}
    }
    return row;
}

static void
ShowRange(DATA * first, DATA * last)
{
    DATA *np = first;
    while (np != last) {
	if (ShowValue(np, (int *) 0, FALSE) >= screen_full)
	    break;
	np = np->next;
    }
}

static void
ShowFrom(DATA * first)
{
    ShowRange(first, (DATA *) 0);
}

/*
 * (Re)display the status line at the top of the screen.
 */
static void
ShowStatus(DATA * np, int opened)
{
    int seq = CountData(np);
    int top = CountData(top_data);
    DATA *last = EndOfData();

    if (!show_error && np != 0 && isVisible()) {
	char buffer[BUFSIZ];

	screen_set_bold(TRUE);
	screen_set_position(0, 0);
	screen_clear_endline();

	(void) sprintf(buffer, "%d of %d", seq, CountData(last));

	screen_set_position(0, screen_cols_left((int) strlen(buffer)));
	screen_puts(buffer);
	screen_set_position(0, 0);

	if (opened < 0) {
	    screen_puts("Edit comment (press return to exit)");
	} else if (opened > 0) {
	    unsigned j;
	    screen_puts("Open-line expecting operator ");
	    for (j = 0; j < SIZEOF(Commands); j++) {
		int c = Commands[j].command;
		if (!isUnary(c) && FirstData(np))
		    continue;
		if ((c = Commands[j].command) == L_PAREN)
		    continue;
		if ((c == R_PAREN) && (opened < 2))
		    continue;
		screen_putc(c);
	    }
	    screen_puts(" or oO to cancel");
	} else if (np->cmd != EOS) {	/* editing a value */
	    unsigned j;
	    for (j = 0; j < SIZEOF(Commands); j++) {
		if (Commands[j].command == np->cmd) {
		    screen_printf("  %s", Commands[j].explain);
		    break;
		}
	    }
	    screen_set_position(0, 5 + use_width);
	    putval(last->sum);
	    screen_puts(" -- total");
	}
	screen_set_bold(FALSE);
    }
    if (isVisible())
	screen_set_position(seq - top + 1, 0);
}

/*
 * Show text in the status line
 */
static void
ShowInfo(const char *msg)
{
    if (screen_active) {	/* we've started curses */
	screen_message("%s", msg);
    } else {
	(void) printf("%s\n", msg);
    }
}

/*
 * Show an error-message in the status line
 */
static void
ShowError(const char *msg, const char *arg)
{
    static const char format[] = "?? %s \"%s\"";

    if (screen_active) {	/* we've started curses */
	screen_message(format, msg, arg);
	show_error = TRUE;
    } else {
	(void) fprintf(stderr, format, msg, arg);
	(void) fprintf(stderr, "\n");
	failed(ADD_PROGRAM);
    }
}

/*
 * Returns true if a file exists, -true if it isn't a file, false if neither.
 */
static int
Fexists(const char *path)
{
    struct stat sb;
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
static int
Ok2Write(const char *path)
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
static void
PutScript(const char *path)
{
    DATA *np;
    FILE *fp = (path && *path) ? fopen(path, "w") : 0;
    char buffer[MAXBFR];
    int count = 0;

    if (fp == 0) {
	ShowError("Cannot open output", path);
	return;
    }

    (void) sprintf(buffer, "Writing results to \"%s\"", path);
    ShowInfo(buffer);

    for (np = all_data->next; np != 0; np = np->next) {
	if (np->cmd == EOS && np->next == 0)
	    break;
	(void) fprintf(fp, "%c", np->cmd);
	if (np->psh)
	    (void) fprintf(fp, "%c", L_PAREN);
	else if (np->cmd != R_PAREN)
	    (void) fprintf(fp, "%s", Format(buffer, np->val));
	if (!np->psh)
	    (void) fprintf(fp, "\t%s", Format(buffer, np->sum));
	if (np->cmd == OP_INT
	    || np->cmd == OP_TAX)
	    (void) fprintf(fp, "\t%s", Format(buffer, np->aux));
	if (np->txt != 0)
	    (void) fprintf(fp, "\t#%s", np->txt);
	(void) fprintf(fp, "\n");
	count++;
    }
    (void) fclose(fp);

    /* If we've written the specified output, reset the changed-flag */
    if (!strcmp(path, top_output))
	scriptCHG = FALSE;

    (void) sprintf(buffer, "Wrote %d line%s to \"%s\"",
		   count, count != 1 ? "s" : "", path);
    ShowInfo(buffer);
    show_error++;		/* force the message to stay until next char */
}

/*
 * Check file-access for reading a script.
 */
static int
Ok2Read(const char *path)
{
    if (Fexists(path) != TRUE || access(path, 04) != 0)
	ShowError("No read access", path);
    else
	return TRUE;
    return FALSE;
}

/*
 * Save the current script-state and nest a new one.
 */
static void
PushScripts(const char *script)
{
    SSTACK *p = SALLOC(SSTACK);

    if (p == 0)
	failed("PushScripts");

    assert(p != 0);

    p->next = sstack;
    p->sfp = scriptFP;
    p->sscripts = scriptv;
    sstack = p;

    scriptFP = 0;
    scriptv = (char **) calloc(2, sizeof(char *));
    scriptv[0] = AllocString(script);
}

/*
 * Restore a previous script-state, if any.
 */
static int
PopScripts(void)
{
    SSTACK *p;

    scriptFP = 0;
    scriptv = 0;
    if ((p = sstack) != 0) {
	scriptFP = p->sfp;
	scriptv = p->sscripts;
	sstack = p->next;
    }
    return (scriptFP != 0);
}

/*
 * On end-of-file, go to the next script (or resume the parent script)
 */
static void
NextScript(void)
{
    (void) fclose(scriptFP);
    scriptFP = 0;
    if (!*(++scriptv))
	PopScripts();
}

/*
 * Read from a script, checking for end-of-file, and performing control-char
 * conversion.
 */
static int
ReadScript(void)
{
    int c = fgetc(scriptFP);
    if (feof(scriptFP) || ferror(scriptFP)) {
	NextScript();
	if (!scriptNUM++)
	    scriptCHG = FALSE;
	c = EOF;
    } else if (c == '^') {
	c = ReadScript();
	if (c == EOF)
	    c = '^';		/* we'll get an EOF on the next call */
	else if (c == '\n')
	    scriptLine++;
	else if (c == '?')
	    c = '\177';		/* delete */
	else
	    c &= 037;
    }
    return c;
}

#define IsScript() (scriptv != NULL && *scriptv != NULL)

/*
 * As long as there is another input-script to process, read it.  Scripts are
 * formatted
 *	<operator><value><tab><ignored>
 * to permit line-oriented entries.
 */
static int
GetScript(void)
{
    static int first;
    static int valued;
    static int ignored;
    static int comment;
    int c;

    while (IsScript()) {
	int was_invisible = !isVisible();

	if (scriptFP == 0) {
	    scriptFP = fopen(*scriptv, "r");
	    if (scriptFP == 0) {
		ShowError("Cannot read", *scriptv);
		scriptv++;
	    } else {
		ShowInfo("Reading script");
		first = TRUE;
		valued = FALSE;
		scriptLine = 1;
		scriptErrs = 0;
	    }
	    continue;
	}
	while (scriptFP != 0) {
	    if ((c = ReadScript()) == EOF)
		continue;
	    if (c == '#' || c == COLON) {
		comment = TRUE;
		ignored = FALSE;
	    } else if (!comment) {
		/*
		 * We pay attention only to the first character or number on a
		 * given input line.
		 */
		if (c == '\t') {
		    ignored = TRUE;
		} else if (valued && c == ' ') {
		    ignored = TRUE;
		}
	    }
	    if (isReturn(c)) {
		first = TRUE;
		valued = FALSE;
	    }
	    if (ignored && isReturn(c)) {
		ignored = FALSE;
	    } else if (!ignored) {
		if (isReturn(c)) {
		    comment = FALSE;
		} else if (first) {
		    if (isdigit(UCH(c))) {
			ungetc(c, scriptFP);
			c = OP_ADD;
		    }
		    first = FALSE;
		}
		if (isdigit(UCH(c)) || (c) == sep_radix) {
		    valued = TRUE;
		}
		return (c);
	    }
	}
	/* Finally, paint the screen if I wasn't doing so before */
	if (was_invisible) {
	    int editcols[3];
	    DATA *last = EndOfData();
	    ShowStatus(last, FALSE);
	    ShowRange(top_data, last);
	    ShowValue(last, editcols, FALSE);
	    first = TRUE;
	    valued = FALSE;
	    return EQUALS;	/* flush out the last line */
	}
    }

    valued = FALSE;
    comment = FALSE;
    ignored = FALSE;

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
static int
GetC(void)
{
    int c;

    if (IsScript()) {
	if (had_error) {
	    had_error = FALSE;
	    if (++scriptErrs >= 3) {
		screen_finish();
		fprintf(stderr, "%s:%d: invalid input\n", *scriptv, scriptLine);
		exit(EXIT_FAILURE);
	    }
	} else {
	    scriptErrs = 0;
	}
    }
    if ((c = GetScript()) == EOS) {
	show_error = FALSE;
	c = screen_getc();
    }
    return (c);
}

/*
 * Given a string, offset into it and insert-position, delete the character at
 * that offset, both from the string and screen.  Return the resulting offset.
 */
static int
DeleteChar(char *buffer, int offset, int pos, int limit)
{
    char *t;

    /* delete from the actual buffer */
    for (t = buffer + offset; (t[0] = t[1]) != EOS; t++) ;

    if (isVisible()) {		/* update the display */
	int y = screen_row();
	int x = screen_col();	/* get current insert-position */
	int col = x - pos + offset;	/* assume pos < len, offset < len */
	screen_set_position(y, col);
	screen_delete_char();
	if (limit > 0 && (int) strlen(buffer) < limit) {
	    screen_set_position(y, col - offset);
	    screen_insert_char(' ');
	    x++;
	}
	if (pos > offset)
	    x--;
	screen_set_position(y, x);
    }
    if (pos > offset)
	pos--;
    return pos;
}

/*
 * Insert a character into the given string, returning the updated insert
 * position.  If the "rmargin" parameter is nonzero, we keep the buffer
 * right-justified to that limit.
 */
static int
InsertChar(char *buffer, int chr, int pos, int lmargin, int rmargin, int *offset)
{
    int len = (int) strlen(buffer);
    char *t;

    /* perform the actual insertion into the buffer */
    for (t = buffer + len;; t--) {
	t[1] = t[0];
	if (t == buffer + pos)
	    break;
    }
    t[0] = (char) chr;

    if (isVisible()) {		/* update the display on the screen */
	int y = screen_row();
	int x = screen_col();
	if (screen_cols_left(x) > 0) {
	    if (rmargin > 0) {
		x--;
		screen_set_position(y, x - pos);
		screen_delete_char();
		screen_set_position(y, x);
	    }
	    screen_insert_char(chr);
	    screen_set_position(y, x + 1);
	} else if (offset != 0) {
	    screen_set_position(y, lmargin);
	    screen_delete_char();
	    screen_set_position(y, x - 1);
	    screen_insert_char(chr);
	    screen_set_position(y, x);
	    *offset += 1;
	} else {
	    HadError();
	}
    }
    return pos + 1;
}

/*
 * Delete from the buffer the character to the left of the given col-position.
 */
static int
doDeleteChar(char *buffer, int col, int limit)
{
    if (col > 0) {
	col = DeleteChar(buffer, col - 1, col, limit);
    } else {
	HadError();
    }
    return col;
}

/*
 * Returns the index of the decimal-point in the given buffer (or -1 if not
 * found).
 */
static int
DecimalPoint(char *buffer)
{
    char *dot = strchr(buffer, sep_radix);

    if (dot != 0)
	return (int) (dot - buffer);
    return -1;
}

/*
 * Return the sequence-pointer of the left-parenthesis enclosing the given
 * operand-set at 'np'.
 */
static DATA *
Balance(DATA * np, int level)
{
    int target = level;

    while (np->prev != 0) {
	if (np->cmd == R_PAREN)
	    level++;
	else if (np->psh)
	    level--;
	if (level <= target)
	    break;		/* unbalanced */
	np = np->prev;
    }
    return (level == 0) ? np : 0;
}

static void
ShowScriptName(void)
{
    while (screen_move_left(screen_col() + 1, 0) > 0) {
	;
    }
    if (*top_output) {
	screen_printf("script: %s", top_output);
    } else {
	screen_puts("no script");
    }
    screen_clear_endline();
    (void) screen_getc();
}

static void
init_editor(char *buffer, int length, int *end, int *offset, int *lmargin)
{
    *offset = 0;
    *lmargin = screen_col();
    *end = screen_cols_left(*lmargin);
    *end = min(*end, length);
    if (*end < (int) strlen(buffer))
	*offset = (int) strlen(buffer) - *end;
}

static void
repaint_screen(DATA * np)
{
    int top, seq;

    seq = CountData(np);
    top = seq - screen_full + 1;
    top = max(top, 1);
    top_data = FindData(top);
    ShowFrom(top_data);
}

/*
 * Edit an arbitrary buffer, starting at the current screen position.
 */
static void
EditBuffer(char *buffer, int length, DATA * np)
{
    int end;
    int col = (int) strlen(buffer);
    int done = FALSE;
    int offset;
    int lmargin;
    int shown = FALSE;

    init_editor(buffer, length, &end, &offset, &lmargin);
    while (!done) {
	int chr;

	while (col - offset < 0) {
	    offset--;
	    shown = FALSE;
	}
	while (col - offset > end) {
	    offset++;
	    shown = FALSE;
	}
	if (isVisible() && !shown) {
	    int x;
	    screen_set_position(screen_row(), lmargin);
	    screen_printf("%.*s", end, buffer + offset);
	    if (screen_cols_left(x = screen_col()) > 0) {
		screen_set_position(screen_row(), x);
		screen_clear_endline();
	    }
	    screen_set_position(screen_row(), lmargin + col - offset);
	    shown = TRUE;
	}
	chr = GetC();
	if (isReturn(chr)) {
	    done = TRUE;
	} else if (isAscii(chr) && isprint(UCH(chr))) {
	    if ((int) strlen(buffer) < length - 1) {
		col = InsertChar(buffer, chr, col, lmargin, 0, &offset);
	    } else {
		HadError();
	    }
	} else if (chr == CTL('L')) {
	    init_editor(buffer, length, &end, &offset, &lmargin);
	    repaint_screen(np);
	} else if (is_delete_left(chr)) {
	    col = doDeleteChar(buffer, col, 0);
	} else if (is_left_char(chr)) {
	    col = screen_move_left(col, 0);
	} else if (is_right_char(chr)) {
	    col = screen_move_right(col, (int) strlen(buffer));
	} else if (is_home_char(chr)) {
	    while (col > 0)
		col = screen_move_left(col, 0);
	} else if (is_end_char(chr)) {
	    while (col < (int) strlen(buffer))
		col = screen_move_right(col, (int) strlen(buffer));
	} else {
	    HadError();
	}
    }
}

/*
 * Edit the comment-field
 */
static void
EditComment(DATA * np)
{
    char buffer[BUFSIZ];
    int row;
    int editcols[3];

    (void) strcpy(buffer, np->txt != 0 ? np->txt : "");

    ShowStatus(np, -1);
    row = ShowValue(np, editcols, TRUE) + 1;
    if (isVisible()) {
	screen_set_position(row, editcols[1]);
	screen_puts(" # ");
    }
    EditBuffer(buffer, sizeof(buffer), np);
    TrimString(buffer);

    if (*buffer != EOS || (np->txt != 0)) {
	if (np->txt != 0) {
	    if (!strcmp(buffer, np->txt))
		return;		/* no change needed */
	    free(np->txt);
	}
	np->txt = (*buffer != EOS) ? AllocString(buffer) : 0;
	scriptCHG = TRUE;
    }
}

/*
 * Returns true if the given entry has editable data
 */
static int
HasData(DATA * np)
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
static int
EditValue(DATA * np, int *len_, Value * val_, int edit)
{
    int c;
    int row;
    int col;			/* current insert-position */
    int editcols[3];
    int done = FALSE;
    int was_visible = isVisible();
    int lmargin = screen_col();
    int nesting;		/* if we find left parenthesis rather than number */
    char buffer[MAXBFR];	/* current input value */

    static char old_digit = EOS;	/* nonzero iff we have pending digit */

    if (np == 0) {
	*len_ = 0;
	return 'q';
    }

    if (np->cmd == R_PAREN)
	edit = FALSE;

    ShowStatus(np, FALSE);
    row = ShowValue(np, editcols, FALSE) + 1;

    if (isVisible()) {
	screen_set_position(row, editcols[0]);
	screen_set_reverse(TRUE);
    }

    if (edit) {
	if (np->psh) {
	    buffer[0] = L_PAREN;
	    buffer[1] = EOS;
	} else {
	    int len, dot;
	    char *s;

	    (void) sprintf(buffer, "%0*.0f", len_frac, np->val);
	    len = (int) strlen(buffer);
	    s = buffer + len;
	    s[1] = EOS;
	    for (c = 0; c < len_frac; c++, s--)
		s[0] = s[-1];
	    dot = len - len_frac;
	    buffer[dot] = sep_radix;
	}
	if (isVisible()) {
	    screen_set_position(row, (int) (editcols[0]
					    + use_width
					    - (int) strlen(buffer)));
	    screen_puts(buffer);
	}
    } else {
	buffer[0] = EOS;
    }

    if (isVisible())
	screen_set_position(row, editcols[0] + use_width);
    col = (int) strlen(buffer);
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
	 * If the current operator is a right parenthesis, we must have
	 * an operator following, with no data intervening:
	 */
	if (np->cmd == R_PAREN) {
	    if (isDigit(c)
		|| (c == sep_radix)
		|| (c == L_PAREN)) {
		HadError();
	    } else {
		*len_ = -2;
		done = TRUE;
	    }
	}
	/*
	 * Move left/right within the buffer to adjust the insertion
	 * position. In curses mode, CTL/F and CTL/B are conflicting.
	 */
	else if (is_left_char(c) && !is_up_page(c)) {
	    col = screen_move_left(col, 0);
	} else if (is_right_char(c) && !is_down_page(c)) {
	    col = screen_move_right(col, (int) strlen(buffer));
	} else if (is_home_char(c)) {
	    while (col > 0)
		col = screen_move_left(col, 0);
	} else if (is_end_char(c)) {
	    while (col < (int) strlen(buffer))
		col = screen_move_right(col, (int) strlen(buffer));
	}
	/*
	 * Backspace deletes the last character entered:
	 */
	else if (is_delete_left(c)) {
	    col = doDeleteChar(buffer, col, use_width);
	    if (*buffer == EOS)
		nesting = FALSE;
	}
	/*
	 * A left parenthesis may be used only as the first (and only)
	 * character of the operand.
	 */
	else if (c == L_PAREN) {
	    if (*buffer != EOS) {
		HadError();
	    } else {
		col = InsertChar(buffer, c, col, lmargin, use_width, (int *) 0);
		nesting = TRUE;
	    }
	}
	/*
	 * If we have received a left parenthesis, and do not delete
	 * it, the next character begins a new operand-line:
	 */
	else if (nesting) {
	    if (UnaryConflict(np, c))
		HadError();
	    else {
		if (isDigit(c) || c == sep_radix) {
		    old_digit = (char) c;
		    c = OP_ADD;
		}
		*len_ = -1;
		done = TRUE;
	    }
	}
	/*
	 * Otherwise, we assume we have a normal value which we are
	 * decoding:
	 */
	else if (isDigit(c)) {
	    int limit = use_width;
	    if (strchr(buffer, sep_radix) == 0)
		limit -= (1 + len_frac);
	    if ((int) strlen(buffer) > limit) {
		HadError();
	    } else {
		col = InsertChar(buffer, c, col, lmargin, use_width, (int *) 0);
	    }
	}
	/*
	 * Decimal point can be entered once for each number. If we
	 * get another, simply move it to the new position.
	 */
	else if (c == sep_radix) {
	    int dot;
	    if ((dot = DecimalPoint(buffer)) >= 0)
		col = DeleteChar(buffer, dot, col, use_width);
	    col = InsertChar(buffer, c, col, lmargin, use_width, (int *) 0);
	}
	/*
	 * Otherwise, we assume a new operator-character for the
	 * next command, flushing out the current command.  Decode
	 * the number (if any) which we have read:
	 */
	else if (c != sep_group) {
	    if (*buffer != EOS) {
		int len = (char) strlen(buffer);
		int dot;
		Value cents;

		if ((dot = DecimalPoint(buffer)) < 0)
		    buffer[dot = len++] = sep_radix;
		while ((len - dot) <= len_frac)
		    buffer[len++] = '0';
		len = dot + 1 + len_frac;	/* truncate */
		buffer[len] = EOS;
		(void) sscanf(&buffer[dot + 1], "%lf", &cents);
		if (dot) {
		    buffer[dot] = EOS;
		    (void) sscanf(buffer, "%lf", val_);
		    buffer[dot] = sep_radix;
		    *val_ *= val_frac;
		} else
		    *val_ = 0.0;
		*val_ += cents;
	    } else {
		*val_ = np->val;
	    }
	    *len_ = (*buffer == L_PAREN) ? 0 : (char) strlen(buffer);
	    done = TRUE;
	}
    }
    if (was_visible)
	screen_set_reverse(FALSE);
    return (c);
}

/*
 * Return true if (given the length returned by 'EditValue()', and the
 * state of the data-list) we don't display a value.
 */
static int
NoValue(int len)
{
    return ((len == 0 || len == -2) && (all_data->next->next != 0));
}

/*
 * Compute one stage of the running total.  To support parentheses, we use two
 * arguments: 'old' is the operand which contains the left parenthesis.
 */
static int
Calculate(DATA * np, DATA * old)
{
    Bool same;
    Value before = np->sum;

    np->sum = (old->prev) ? old->prev->sum : 0.0;
    switch (old->cmd) {
    default:
    case OP_ADD:
	np->sum += np->val;
	break;
    case OP_SUB:
	np->sum -= np->val;
	break;
    case OP_NEG:
	np->sum = -np->sum;
	break;
    case OP_MUL:
	np->sum *= (np->val / val_frac);
	np->sum = Floor(np->sum);
	break;
    case OP_DIV:
	if (np->val == 0.0) {
	    if (np->sum > 0.0)
		np->sum = (Value) big_long;
	    else if (np->sum < 0.0)
		np->sum = (Value) (-big_long);
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
static void
Recompute(DATA * base)
{
    DATA *np = base;
    DATA *op;
    int level = LevelOf(np);
    Bool same;

    while (np != 0) {
	if (np->psh) {
	    np->sum = 0.0;
	    level++;
	} else {
	    if (np->cmd == R_PAREN) {
		level--;
		np->val = np->prev->sum;
		op = Balance(np, level);
		if (op == 0) {
		    op = all_data;
		    level = 0;
		}
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
static DATA *
OpenLine(DATA * base, int after, int *repeated, int *edit)
{
    DATA *save_top = top_data;
    DATA *op = after ? base : base->prev;
    DATA *np;
    int this_row;
    int done = FALSE;
    int nested;

    np = AllocData(op);
    nested = LevelOf(np);
    this_row = CountFromTop(np);

    /* Adjust 'top_data' if we have to scroll a little */
    if (this_row <= 1) {
	while (this_row++ <= 1) {
	    if (top_data->prev == all_data)
		break;
	    top_data = top_data->prev;
	}
    } else {
	this_row -= (screen_full - 2);
	while (this_row-- > 0)
	    top_data = top_data->next;
    }

    /* (Re)display the screen with the opened line */
    if (top_data == save_top)
	ShowFrom(np->next);
    else
	ShowFrom(top_data);
    ShowStatus(np, TRUE + nested);
    screen_clear_endline();
    *edit = FALSE;		/* assume we'll get some new data */

    while (!done) {
	int chr = GetC();
	if (UnaryConflict(np, chr)) {
	    HadError();
	    continue;
	}
	switch (chr) {
	case OP_INT:		/* sC: open to interest */
	case OP_TAX:		/* sC: open to sales tax */
	    /* patch: provide defaults */
	case OP_ADD:		/* sC: open to add */
	case OP_SUB:		/* sC: open to subtract */
	case OP_NEG:		/* sC: open to negate */
	case R_PAREN:		/* sC: open closing brace */
	    setval(np, chr, 0.0, FALSE);
	    done++;
	    break;
	case L_PAREN:
	    setval(np, OP_ADD, 0.0, TRUE);
	    *edit = TRUE;	/* force this to display */
	    done++;
	    break;
	case OP_MUL:		/* sC: open to multiply */
	case OP_DIV:		/* sC: open to divide */
	    setval(np, chr, val_frac, FALSE);
	    done++;
	    break;
	case 'a':
	case 's':
	case 'n':
	case 'm':
	case 'd':
	case 'i':
	case 't':
	    chr = isRepeats(chr);
	    setval(np, chr, LastVAL(np, chr), FALSE);
	    done++;
	    *repeated = TRUE;
	    break;
	case 'q':
	case 'Q':
	case 'o':
	case 'O':
	case 'x':
	case 'X':
	case 'u':
	case 'U':
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
	    HadError();
	    continue;
	}
    }
    return (np);
}

/*
 * Perform half/full-screen scrolling:
 */
static DATA *
ScrollBy(DATA * np, int amount)
{
    int last_seq = CountAllData();
    int this_seq = CountData(np);
    int next_seq;
    int top = CountData(top_data);

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
	    next_seq = top + amount;
	    next_seq = max(next_seq, 1);
	    top = next_seq;
	}
    }
    ShowFrom(top_data = FindData(top));
    return FindData(next_seq);
}

/*
 * Compute a one-line movement of the cursor.  The 'amount' argument
 * compensates for other adjustments to the current data pointer in the calling
 * functions.
 */
static DATA *
JumpBy(DATA * np, int amount)
{
    int this_seq = CountData(np);
    int next_seq = this_seq + amount;
    int last_seq = CountAllData();
    Bool end_flag = TRUE;
    Bool un_moved = FALSE;

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
	int top = CountData(top_data);
	Bool adjust = TRUE;
	if (next_seq < top)
	    top_data = np;
	else if (next_seq >= top + screen_full)
	    top_data = FindData(next_seq - screen_full + 1);
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
static DATA *
JumpTo(DATA * np, int seq)
{
    return JumpBy(np, seq - CountData(np));
}

/*
 * Prompt/process a :-command
 */
static DATA *
ColonCommand(DATA * np)
{
    DATA *save_top = top_data;
    DATA *prior_np = np;
    char buffer[BUFSIZ];
    char *reply;
    static const char *last_write = "";

    if (CountFromTop(np) >= screen_full - 1) {
	top_data = top_data->next;
	ShowFrom(top_data);
    }
    ShowStatus(np, FALSE);	/* in case we have multiple prompts */

    screen_set_position(screen_full, 0);
    screen_putc(COLON);
    screen_putc(' ');
    *buffer = EOS;
    EditBuffer(buffer, sizeof(buffer), np);
    TrimString(buffer);
    reply = buffer;
    while (isspace(UCH(*reply)))
	reply++;

    if (*reply != EOS) {
	if (isdigit(UCH(*reply))) {
	    char *dst;
	    int seq = (int) strtol(reply, &dst, 0);
	    np = JumpTo(np, seq);
	} else {
	    const char *param = reply + 1;
	    while (isspace(UCH(*param)))
		param++;
	    switch (*reply) {
	    case '$':		/* FALLTHRU */
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
		if (Ok2Read(param))
		    PushScripts(param);
		break;
	    case 'f':
		ShowScriptName();
		break;
	    case 'r':
		if (Ok2Read(param))
		    PushScripts(param);
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
	    case 'x':
		show_scripts = TRUE;
		break;
	    default:
		HadError();
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
static int
ScreenMovement(DATA ** pp, int chr)
{
    DATA *np = *pp;
    int ok = TRUE;

    if (chr == COLON) {
	np = ColonCommand(np);
    } else if (chr == 'z') {
	chr = GetC();
	if (isReturn(chr)) {
	    top_data = np;
	} else {
	    int top;
	    int seq = CountData(np);
	    if (chr == '-') {	/* use current entry as end */
		top = seq - screen_full + 1;
	    } else {
		top = seq - screen_half + 1;
	    }
	    top = max(top, 1);
	    top_data = FindData(top);
	}
	ShowFrom(top_data);
#ifdef KEY_HOME
    } else if (chr == KEY_HOME) {	/* C: move to first entry in list */
	np = all_data->next;
	ShowFrom(top_data = np);
#endif
#ifdef KEY_END
    } else if (chr == KEY_END) {	/* C: move to last entry in list */
	int top, seq;

	np = EndOfData();
	seq = CountData(np);
	top = seq - screen_full + 1;
	top = max(top, 1);
	top_data = FindData(top);
	ShowFrom(top_data);
#endif
    } else if (chr == CTL('L')) {
	repaint_screen(np);
    } else if (chr == 'H') {	/* C: move to first entry on screen */
	np = top_data;
    } else if (chr == 'L') {	/* C: move to last entry on screen */
	np = ScreenBottom();
    } else if (is_down_char(chr)) {	/* C: move down 1 line */
	np = JumpBy(np, 1);
    } else if (is_up_char(chr)) {	/* C: move up 1 line */
	np = JumpBy(np, -1);
    } else if (chr == CTL('D')) {	/* C: scroll forward 1/2 screen */
	np = ScrollBy(np, screen_half);
    } else if (chr == CTL('U')) {	/* C: scroll backward 1/2 screen */
	np = ScrollBy(np, -screen_half);
    } else if (is_down_page(chr)) {	/* C: scroll forward one screen */
	np = ScrollBy(np, screen_full);
    } else if (is_up_page(chr)) {	/* C: scroll backward one screen */
	np = ScrollBy(np, -screen_full);
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
static void
ShowHelp(void)
{
    DATA *save_data = all_data;
    DATA *save_top = top_data;
    DATA *np = 0;
    int end;
    int done = FALSE;
    char buffer[BUFSIZ];

    if ((all_data = all_help) == 0) {
	FILE *fp;

	np = AllocData((DATA *) 0);	/* header line not shown */

	assert(all_data != 0);

	if ((fp = fopen(helpfile, "r")) != 0) {
	    while (fgets(buffer, sizeof(buffer), fp) != 0) {
		np = AllocData(np);
		np->txt = AllocString(buffer);
	    }
	    (void) fclose(fp);
	} else {
	    np = AllocData(np);
	    np->txt =
		AllocString("Could not find help-file.  Press 'q' to exit.");
	}
    }

    np = top_data = all_data->next;
    end = CountAllData();
    ShowFrom(all_data);

    while (!done) {
	int chr;

	screen_set_position(0, 0);
	screen_set_bold(TRUE);
	screen_clear_endline();

	(void) sprintf(buffer, "line %d of %d", CountData(np), end);
	screen_set_position(0, screen_cols_left((int) strlen(buffer)));
	screen_puts(buffer);

	screen_set_position(0, 0);
	screen_printf("ADD %s -- %s -- ", RELEASE, copyrite);
	screen_set_bold(FALSE);
	screen_set_position(CountFromTop(np) + 1, 0);

	chr = GetC();
	if (chr == 'q' || chr == 'Q') {
	    done = TRUE;
	} else if (!ScreenMovement(&np, chr)) {
	    HadError();
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

static int
AbsolutePath(const char *path)
{
#if	!defined(unix) && !defined(vms)		/* assume MSDOS */
    if (isalpha(UCH(*path)) && path[1] == ':')
	path += 2;
#endif
    return isSlash(*path)
	|| ((*path++ == '.')
	    && (isSlash(*path)
		|| (*path++ == '.' && isSlash(*path))));
}

static char *
PathLeaf(char *path)
{
    int n;
    for (n = (int) strlen(path); n > 0; n--)
	if (isSlash(path[n - 1]))
	    break;
    return path + n;
}
#endif

#ifndef ADD_HELPFILE
#define ADD_HELPFILE ADD_PROGRAM ".hlp"
#endif

/*
 * Find the help-file.  On UNIX and MSDOS, we look for the file in the same
 * directory as that in which this program is stored, located by searching the
 * PATH environment variable.
 */
static void
FindHelp(const char *program)
{
    char temp[BUFSIZ];
    const char *tail = ADD_HELPFILE;
    char *s = strcpy(temp, program);

# if SYS_VMS
    if (strcspn(tail, "[]:") != strlen(tail)) {
	strcpy(temp, tail);
	s = temp + strlen(temp);
	tail = "";
    } else {
	for (s += strlen(temp); s != temp; s--)
	    if (s[-1] == ']' || s[-1] == ':')
		break;
    }
# else /* assume UNIX or MSDOS */
    if (AbsolutePath(tail)) {
	strcpy(temp, tail);
	s = temp + strlen(temp);
	tail = "";
    } else if (AbsolutePath(temp)) {
	s = PathLeaf(temp);
    } else {
	const char *path = getenv("PATH");
	int j = 0, k;

	if (path == 0)
	    path = "";
	while (path[j] != EOS) {
	    int l;
	    for (k = j; path[k] != EOS && path[k] != PATHSEP; k++)
		temp[k - j] = path[k];
	    if ((l = k - j) != 0)
		temp[l++] = '/';
	    s = strcpy(temp + l, program);
	    if (access(temp, 5) == 0) {
		temp[l] = EOS;
		break;
	    }
	    j = (path[k] != EOS) ? k + 1 : k;
	}
	if (path[j] == EOS) {
	    s = temp;
	    *s++ = '.';
	    *s++ = '/';
	    s = PathLeaf(strcpy(s, program));
	}
    }
# endif	/* VMS/UNIX/MSDOS */
    (void) strcpy(s, tail);
    helpfile = AllocString(temp);
}

/*
 * Main program loop: given 'old' operator (applies to current entry), read the
 * value 'val', delimited by the next operator 'chr'.
 */
static int
Loop(void)
{
    DATA *np = EndOfData();
    Value val;
    int test_c;
    int len;
    int opened;
    int repeated;		/* if true, 'Loop' assumes editable value */
    int edit = FALSE;

    for (;;) {
	int chr = EditValue(np, &len, &val, edit);
	switch (chr) {
	case '\t':
	    chr = EQUALS;
	    break;
	case CTL('P'):
	    chr = 'k';
	    break;
	case CTL('N'):
	case '\r':
	case '\n':
	    chr = 'j';
	    break;
	case ' ':
	    chr = DefaultOp(np);
	    break;
	}

	if (chr == CTL('G')) {
	    ShowScriptName();
	} else if (chr == 'X') {	/* C: delete current entry, move up */
	    if (NoValue(len)) {
		np = FreeData(np, TRUE);
		np = JumpBy(np, -1);
		edit = HasData(np);
	    } else {
		edit = FALSE;
	    }
	} else if (chr == 'x') {	/* C: delete current entry, move down */
	    if (NoValue(len)) {
		np = FreeData(np, TRUE);
		edit = HasData(np);
	    } else {
		edit = FALSE;
	    }
	} else if (chr == 'u') {	/* C: undo last 'x' command */
	    if (HasData(np))
		edit = TRUE;
	    else
		HadError();
	} else if ((test_c = isToggles(chr)) != EOS) {
	    /* C: toggle current operator */
	    chr = test_c;
	    if (UnaryConflict(np, chr)) {
		HadError();
	    } else {
		if (len == 0)
		    val = np->val;
		else
		    edit = TRUE;
		setval(np, chr, val, np->psh);
	    }
	} else {
	    if (len != 0) {
		setval(np, np->cmd, val, (len == -1));
		if (LastData(np) && isCommand(chr)) {
		    (void) AllocData(np);
		    np->next->cmd = (char) chr;
		} else if (LastData(np) && isRepeats(chr)) {
		    (void) AllocData(np);
		    np->next->cmd = (char) isRepeats(chr);
		}
		Recompute(np);
	    } else {
		(void) ShowValue(np, (int *) 0, FALSE);
	    }

	    repeated = FALSE;
	    opened = FALSE;

	    if (chr == '?') {	/* C: display help file */
		ShowHelp();
	    } else if (chr == '#') {	/* C: edit comment */
		EditComment(np);
	    } else if (chr == 'Q') {	/* C: quit w/o changes */
		return (FALSE);
	    } else if (chr == 'q') {	/* C: quit */
		return (TRUE);
	    } else if (ScreenMovement(&np, chr)) {
		;
	    } else if (chr == 'O'	/* C: open before */
		       || chr == 'o') {		/* C: open after */
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
		    np->cmd = (char) chr;
		} else if (chr == 'w' && use_width < max_width) {
		    ++use_width;
		    ShowFrom(top_data);
		} else if (chr == 'W' && use_width > 6) {
		    --use_width;
		    ShowFrom(top_data);
		} else if (chr == EQUALS) {
		    Recompute(np);
		} else {
		    HadError();
		}
	    }
	    if (repeated) {
		edit = TRUE;
		setval(np, chr, LastVAL(np, chr), FALSE);
	    } else if (!opened) {
		edit = HasData(np);
	    }
	}
    }
}

static void
usage(void)
{
    static const char *tbl[] =
    {
	"Usage: " ADD_PROGRAM " [options] [scripts]"
	,""
	,"Options:"
	,"  -h           print this message"
	,"  -i interval  specify compounding-interval (default=12)"
	,"  -o script    specify output-script name (default is the first"
	,"               input-script name)"
	,"  -p num       specify precision (default=2)"
	,"  -V           print the version"
	,""
	,"Description:"
	,"  Script-based adding machine that allows you to edit the operations"
	,"  and data."
    };
    unsigned j;

    for (j = 0; j < SIZEOF(tbl); j++)
	fprintf(stderr, "%s\n", tbl[j]);
    exit(EXIT_FAILURE);
}

#if !HAVE_GETOPT
int optind;
int optchr;
char *optarg;

int
getopt(int argc, char **argv, char *opts)
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

int
main(int argc, char **argv)
{
    long k;
    int j;
    int max_digits;		/* maximum length of a number */
    int changed;
    char tmp;

    (void) signal(SIGFPE, SIG_IGN);
    len_frac = 2;
    interval = 12;

#ifdef HAVE_LOCALECONV
    if (setlocale(LC_ALL, "") != NULL) {
	struct lconv *data = localeconv();
	if (data != NULL) {
	    if (!IsEmpty(data->decimal_point))
		sep_radix = *data->decimal_point;
	    if (!IsEmpty(data->thousands_sep))
		sep_group = *data->thousands_sep;
	}
    }
#endif

#ifdef LONG_MAX
    big_long = LONG_MAX;
#else
    /*
     * Compute the maximum number of digits to display:
     *
     *      big_long - the maximum positive value that we can stuff into
     *                 a 'long'. Assume symmetry, i.e., that we can use
     *                 the same negative magnitude.
     *      max_width - the maximum length of a formatted number, counting
     *                 sign, decimal point and commas between groups
     *                 of digits.
     */
    big_long = k = 1;
    while ((k = (k << 1) + 1) > big_long)
	big_long = k;
#endif

    for (k = big_long, max_digits = 0; k >= 10; k /= 10)
	max_digits++;

    FindHelp(argv[0]);

    while ((j = getopt(argc, argv, "hi:o:p:V")) != -1)
	switch (j) {
	case 'p':
	    if ((sscanf(optarg, "%d%c", &len_frac, &tmp) != 1)
		|| (len_frac <= 0 || len_frac > max_digits - 2)) {
		fprintf(stderr, "Option p limited to 0..%d\n",
			max_digits - 2);
		usage();
		/* NOTREACHED */
	    }
	    break;
	case 'i':
	    if ((sscanf(optarg, "%d%c", &interval, &tmp) != 1)
		|| (interval <= 0 || interval > 100)) {
		fprintf(stderr, "Option i limited to 0..100\n");
		usage();
		/* NOTREACHED */
	    }
	    break;
	case 'o':
	    top_output = optarg;
	    break;
	case 'h':
	    /* FALLTHRU */
	default:
	    usage();
	    /* NOTREACHED */
	case 'V':
	    puts(RELEASE);
	    return EXIT_SUCCESS;
	}

    for (j = 0, val_frac = 1.0; j < len_frac; j++)
	val_frac *= 10.0;

    max_width = 1 + ((max_digits - len_frac) + 2) / 3 + max_digits + 1;
    use_width = max_width;
    if (use_width > 20)
	use_width = 20;

    /*
     * Allocate some dummy data so we can propagate results from it.
     */
    all_data = 0;
    for (j = 0; j < 2; j++) {
	setval(AllocData((DATA *) 0), OP_ADD, 0.0, FALSE);
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
	    (void) Ok2Read(scriptv[j]);
	}
    } else {
	scriptv = argv + argc;	/* points to a null-pointer */
    }

    /*
     * Get the default output-filename
     */
    if (optind < argc && !top_output)
	top_output = argv[argc - 1];

    if (top_output == 0)
	top_output = "";

    /*
     * Setup and run the interactive portion of the program.
     */
    screen_start();
    changed = Loop();
    screen_finish();

    /*
     * If one or more scripts were given as input, and a '-o' argument
     * was given, overwrite the last one with the results.
     */
    if (*top_output && changed && scriptCHG)
	PutScript(top_output);

#if NO_LEAKS
    free(helpfile);
    while (FreeData(all_data, FALSE) != 0)
	/*EMPTY */ ;
#if HAVE_DBMALLOC_H
    free(-1);			/* FIXME: force linux+dbmalloc to report */
#endif
#endif

    return (EXIT_SUCCESS);
}
