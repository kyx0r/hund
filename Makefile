CC = gcc
CFLAGS = --std=c11 -g -Wall -Wextra -pedantic
LIBS = -lncurses
OBJDIR = obj
OBJ = main.o
EXENAME = hund
TESTEXENAME = test/testme

all : project

project : $(OBJDIR)/$(OBJ)
	$(CC) $(LIBS) -o $(EXENAME) $^

$(OBJDIR)/$(OBJ) : src/$(subst .o,.c,$(OBJ)) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

test : $(OBJDIR)/test.o $(subst $(OBJDIR)/main.o,,$(OBJDIR)/$(OBJ))
	$(CC) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME)

.PHONY : clean
clean :
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
