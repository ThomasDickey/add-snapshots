#!/bin/sh
# $Id: xterm.sh,v 1.2 2018/07/01 17:50:31 tom Exp $
xterm -title "ADD $*" -fn '*-fixed-*-*-*-20-*' -xrm 'XTerm*.sunKeyboard:false' -e add $*
