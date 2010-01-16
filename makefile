CC=gcc
CFLAGS=-g
LIBS=-lutil -lreadline
INCLUDES=
YFLAGS=-d
objs=mi_lex.yy.o mi_grammar.tab.o mi_parsetree.o mi_parser.o log.o

all: gdbvim miparser

gdbvim: $(objs) cmd_mapping.o gdbvim.o
	gcc $^ -o $@ $(CFLAGS) $(LIBS)

miparser: $(objs) mi_driver.o
	gcc $^ -o $@ $(CFLAGS)

mi_grammar.tab.c: mi_grammar.y
	bison $(YFLAGS) $^

mi_lex.yy.c: mi_lexer.l mi_grammar.tab.c
	flex mi_lexer.l

cmd_mapping.c: cmd_mapping.gperf
	gperf cmd_mapping.gperf --output-file=cmd_mapping.c

clean:
	- rm *.o
	- rm mi_lex.yy.c mi_grammar.tab.c mi_grammar.tab.h cmd_mapping.c
	- rm gdbvim miparser
