</$objtype/mkfile

TARG=dufus
OFILES=\
	dufus.$O\

HFILES=\
	dufus.h\

BIN=/$objtype/bin
LDFLAGS=-ldraw -l9 -lmemdraw -lmemlayer -lkeyboard -levent

</sys/src/cmd/mkone 