.POSIX:
CC = cc
CFLAGS = --std=c99 -Wall -Wextra -pedantic -Wimplicit-fallthrough=0
LDFLAGS =
EXENAME = hund
TESTEXENAME = testme

.PHONY: all clean test

all: $(EXENAME)

$(EXENAME): main.o fs.o ui.o file_view.o utf8.o task.o terminal.o
	$(CC) $(LDFLAGS) -o $(EXENAME) main.o fs.o ui.o \
		file_view.o utf8.o task.o terminal.o
main.o: main.c task.h ui.h
fs.o: fs.c fs.h
ui.o: ui.c ui.h file_view.h utf8.h terminal.h
file_view.o: file_view.c file_view.h fs.h
task.o: task.c task.h fs.h utf8.h
terminal.o: terminal.c terminal.h utf8.h
utf8.o: utf8.c widechars.h
test.o: test.c

.SUFFIXES: .c .o
	$(CC) $(CFLAGS) -o $<

test: test.o fs.o ui.o file_view.o utf8.o task.o terminal.o
	$(CC) -o $(TESTEXENAME) test.o fs.o ui.o \
		file_view.o utf8.o task.o terminal.o \
		&& ./$(TESTEXENAME) && make $(EXENAME)

clean:
	rm -r testdir &> /dev/null || true
	rm -r *.o &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
