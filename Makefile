CC = gcc
LD = gcc
CFLAGS = --std=c11 -g -Wall -Wextra -pedantic
#LIBS = -lform -lpanel -lncurses
LIBS = -lpanel -lncurses
OBJDIR = obj
OBJ = main.o path.o file.o ui.o
EXENAME = hund
TESTEXENAME = test/testme
TESTSCRIPTNAME = test/test.sh

all : $(EXENAME)

$(EXENAME) : $(addprefix $(OBJDIR)/, $(OBJ))
	$(LD) $(LIBS) $^ -o $(EXENAME) 

$(OBJDIR)/main.o : src/main.c src/include/ui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/path.o : src/path.c src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file.o : src/file.c src/include/file.h src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ui.o : src/ui.c src/include/ui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

test : $(OBJDIR)/test.o $(addprefix $(OBJDIR)/, $(subst main.o,,$(OBJ)))
	$(CC) $(LIBS) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME) && make $(EXENAME) && ./$(TESTSCRIPTNAME)

.PHONY : clean test
clean :
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
