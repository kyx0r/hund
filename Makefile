CC = gcc #musl-gcc, clang
LD = gcc #musl-gcc -static
CFLAGS += --std=c99 -Wall -Wextra -pedantic -g
LDFLAGS =
OBJDIR = obj
OBJ = main.o fs.o ui.o file_view.o utf8.o task.o terminal.o
TESTOBJ := test.o fs.o ui.o file_view.o utf8.o task.o terminal.o
EXENAME = hund
TESTEXENAME = test/testme
TESTSCRIPTNAME = test/test.sh

all : $(EXENAME)

$(EXENAME) : $(addprefix $(OBJDIR)/, $(OBJ))
	$(LD) $(LDFLAGS) $(LIBS) $^ -o $(EXENAME)

$(OBJDIR)/%.o : src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

test : $(addprefix $(OBJDIR)/, $(TESTOBJ))
	$(CC) $(LIBS) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME) && make $(EXENAME)

testex :
	./$(TESTSCRIPTNAME)

.PHONY : all clean test testex
clean :
	rm -r testdir &> /dev/null || true
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
