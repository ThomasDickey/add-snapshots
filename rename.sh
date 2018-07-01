#! /bin/sh
# $Id: rename.sh,v 1.3 2018/07/01 17:06:39 tom Exp $
# install-helper for add's manpage and help-file.
#
# $1 = input file
# $2 = actual name that "add" is installed as
# $2 = output filename
# $4+ = install program and possible options

LANG=C;     export LANG
LC_ALL=C;   export LC_ALL
LC_CTYPE=C; export LC_CTYPE
LANGUAGE=C; export LANGUAGE

SOURCE=$1; shift
BINARY=$1; shift
TARGET=$1; shift

CHR_LEAD=`echo "$BINARY" | sed -e 's/^\(.\).*/\1/'`
CHR_TAIL=`echo "$BINARY" | sed -e 's/^.//'`
ONE_CAPS=`echo $CHR_LEAD | tr '[a-z]' '[A-Z]'`$CHR_TAIL
ALL_CAPS=`echo "$BINARY" | tr '[a-z]' '[A-Z]'`
UNDERLINE=`echo "$BINARY"| sed -e 's/./-/g'`

sed	-e "s,\<fBadd\>,fB$BINARY,g" \
	-e "s,\<fBAdd\>,fB$ONE_CAPS,g" \
	-e "s,\<fBADD\>,fB$ALL_CAPS,g" \
	-e "s,\<ADD\>,$ALL_CAPS,g" \
	-e "s,\<add\>,$BINARY,g" \
	-e "s,---,$UNDERLINE," \
	<$SOURCE >source.tmp
"$@" source.tmp "$TARGET"
rm -f source.tmp
