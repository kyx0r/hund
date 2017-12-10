CC = gcc # clang works too!
LD = gcc
# TODO ld?
# TODO test older C standards
CFLAGS = --std=c11 -Wall -Wextra -pedantic -Wimplicit-fallthrough=0
DEBUG = true
ifeq ($(DEBUG),true)
	CFLAGS += -g
else
	CFLAGS += -O2 -s
endif
LIBS = -lpanel -lncurses
OBJDIR = obj
OBJ = main.o path.o file.o ui.o file_view.o utf8.o task.o
TESTOBJ := test.o path.o file.o ui.o file_view.o utf8.o task.o
EXENAME = hund
TESTEXENAME = test/testme
TESTSCRIPTNAME = test/test.sh

all : $(EXENAME)

$(EXENAME) : $(addprefix $(OBJDIR)/, $(OBJ))
	$(LD) $^ -o $(EXENAME) $(LIBS)

$(OBJDIR)/main.o : src/main.c src/include/ui.h src/include/task.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/utf8.o : src/utf8.c src/include/utf8.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/path.o : src/path.c src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file.o : src/file.c src/include/file.h src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file_view.o : src/file_view.c src/include/file_view.h \
	src/include/file.h src/include/utf8.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ui.o : src/ui.c src/include/ui.h src/include/file_view.h src/include/utf8.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/task.o : src/task.c src/include/task.h src/include/file.h src/include/utf8.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

test : $(addprefix $(OBJDIR)/, $(TESTOBJ))
	$(CC) $(LIBS) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME) && make $(EXENAME)

testex :
	./$(TESTSCRIPTNAME)

.PHONY : all clean test testex
clean :
	rm -r testdir &> /dev/null || true
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
