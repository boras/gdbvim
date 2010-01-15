CC=gcc
CFLAGS=-g
LIBS=-lutil -lreadline
INCLUDES=
YFLAGS=-d
objs=mi_lex.yy.o mi_grammar.tab.o mi_parsetree.o mi_parser.o log.o

all: gdbvim miparser

gdbvim: $(objs) gdbvim.o
	gcc $^ -o $@ $(CFLAGS) $(LIBS)

miparser: $(objs) mi_driver.o
	gcc $^ -o $@ $(CFLAGS)

mi_grammar.tab.c: mi_grammar.y
	bison $(YFLAGS) $^

mi_lex.yy.c: mi_lexer.l mi_grammar.tab.c
	flex mi_lexer.l

clean:
	- rm *.o
	- rm mi_lex.yy.c mi_grammar.tab.c mi_grammar.tab.h
	- rm gdbvim miparser
