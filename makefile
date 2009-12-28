CFLAGS=-g
LIBS=-lutil
INCLUDES=
YFLAGS=-d
objs=lex.yy.o grammar.tab.o parsetree.o parser.o

all: gdbvim parser

gdbvim: $(objs) gdbvim.o
	gcc $^ -o $@ $(CFLAGS) $(LIBS)

parser: $(objs) mi_driver.o
	gcc $^ -o $@ $(CFLAGS)

grammar.tab.c: grammar.y
	bison $(YFLAGS) $^

lex.yy.c: lexer.l grammar.tab.c
	flex lexer.l

clean:
	- rm *.o
	- rm lex.yy.c grammar.tab.c grammar.tab.h
	- rm gdbvim parser
