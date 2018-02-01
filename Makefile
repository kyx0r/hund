CFLAGS = --std=c99 -Wall -Wextra -pedantic -Wimplicit-fallthrough=0
LDFLAGS =
EXENAME = hund
TESTEXENAME = test/testme
TESTSCRIPTNAME = test/test.sh

.PHONY : all clean test testex

all : $(EXENAME)

$(EXENAME) : src/main.o src/fs.o src/ui.o src/file_view.o \
	src/utf8.o src/task.o src/terminal.o
	$(CC) $(LDFLAGS) src/main.o src/fs.o src/ui.o src/file_view.o \
	src/utf8.o src/task.o src/terminal.o -o $(EXENAME)

src/main.o : src/main.c src/include/task.h src/include/ui.h
src/fs.o : src/fs.c src/include/fs.h
src/ui.o : src/ui.c src/include/ui.h src/include/file_view.h \
	src/include/utf8.h src/include/terminal.h
src/file_view.o : src/file_view.c src/include/file_view.h src/include/fs.h
src/task.o : src/task.c src/include/task.h src/include/fs.h src/include/utf8.h
src/terminal.o : src/terminal.c src/include/terminal.h src/include/utf8.h
src/utf8.o : src/utf8.c src/include/widechars.h
test/test.o : test/test.c

test : test/test.o src/fs.o src/ui.o src/file_view.o \
	src/utf8.o src/task.o src/terminal.o
	$(CC) -o $(TESTEXENAME) test/test.o src/fs.o src/ui.o src/file_view.o \
	src/utf8.o src/task.o src/terminal.o && ./$(TESTEXENAME) && make $(EXENAME)

testex :
	./$(TESTSCRIPTNAME)

clean :
	rm -r testdir &> /dev/null || true
	rm -r src/*.o test/*.o &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
