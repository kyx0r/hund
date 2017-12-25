LD = gcc
# TODO ld?
CFLAGS += --std=c99 -Wall -Wextra -pedantic
LDFLAGS =
DEBUG = true
ifeq ($(DEBUG),true)
	CFLAGS += -g
else
	CFLAGS += -O2 -Os -s
endif
LIBS = -lpanel -lncurses
OBJDIR = obj
OBJ = main.o fs.o ui.o file_view.o utf8.o task.o
TESTOBJ := test.o fs.o ui.o file_view.o utf8.o task.o
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
