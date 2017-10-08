CC = gcc
LD = gcc
CFLAGS = --std=c11 -g -Wall -Wextra -pedantic
#LIBS = -lform -lpanel -lncurses
LIBS = -lpanel -lncurses
OBJDIR = obj
OBJ = main.o file_view.o path.o file.o prompt.o
EXENAME = hund
TESTEXENAME = test/testme

all : project

project : $(addprefix $(OBJDIR)/, $(OBJ))
	$(LD) $(LIBS) $^ -o $(EXENAME) 

$(OBJDIR)/main.o : src/main.c src/include/file_view.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file_view.o : src/file_view.c src/include/file_view.h src/include/file.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/path.o : src/path.c src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file.o : src/file.c src/include/file.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/prompt.o : src/prompt.c src/include/prompt.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

test : $(OBJDIR)/test.o $(addprefix $(OBJDIR)/, $(subst main.o,,$(OBJ)))
	$(CC) $(LIBS) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME)

.PHONY : clean test
clean :
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
