#!/bin/sh
# $Id: run_test.sh,v 1.10 2010/07/08 21:07:57 tom Exp $
# regression script for 'add'
if test $# = 0
then
	eval $0 *.add
	exit
fi
#
PATH=`cd ..;pwd`:$PATH; export PATH
#
CASE64=`file ../add 2>/dev/null | fgrep 64-bit`
#
LINES=24;export LINES
COLS=80;export COLS
rm -f un*ble
touch unwritable unreadable
chmod 444 unwritable
chmod 000 unreadable
trap "rm -f un*ble" 0 1 2 5 15
#
EXIT=0
for i in $*
do
	I=`basename $i .add`
	NUL=/dev/null
	OUT="$I.out"
	ERR="$I.err"
	REF="$I.ref"
	TMP="$I.tmp"
	OPT=""
	if test -n "$CASE64"
	then
		test -f "$I-64.ref" && REF="$I-64.ref"
	fi
	if test -f $I.opt
	then
		OPT=`cat $I.opt`
	fi
	rm -f $OUT $ERR $TMP
	# Build a script that's guaranteed to write an output. Some curses
	# implementations don't allow us to simply pipe to the application.
	cp $I.add $TMP
	chmod 644 $TMP
	echo ':w' >>$TMP
	echo 'Q'  >>$TMP
	add $OPT -o $OUT $TMP >$NUL 2>$ERR
	rm -f $TMP
	if test -f $ERR
	then
		if ( fgrep 'Electric Fence' $ERR >/dev/null )
		then
			cat $ERR \
				| sed -e '1,/Electric Fence/d' \
				| tr '\007' @ \
				| sed -e 's/@//g' \
				>>$OUT
		else
			cat $ERR \
				| tr '\007' @ \
				| sed -e 's/@//g' \
				>>$OUT
		fi
		rm -f $ERR
	fi
	if test -f $OUT
	then
		if test -f $REF
		then
			if ( cmp -s $OUT $REF )
			then
				echo '** ok: '$I
				rm -f $OUT
			else
				echo '?? fail: '$I
				diff $REF $OUT 
				EXIT=1
			fi
		else
			echo '...saving '$REF
			mv $OUT $REF
		fi
	else
		echo '? no output for '$I
		exit 1
	fi
done
exit $EXIT
