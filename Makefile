CC = gcc
LD = gcc
CFLAGS = --std=c11 -g -Wall -Wextra -pedantic
LIBS = -Wl,--start-group -ltinfo -lpanel -lncurses -Wl,--end-group
# "-ltinfo" and "-Wl,--start/end-group" are here because
# at some point - most likely after system update (arch ftw)
# I started to get gcc errors:
# undefined reference to symbol 'wtimeout'
# /usr/lib/libtinfo.so.6: error adding symbols: DSO missing from command line
OBJDIR = obj
OBJ = main.o path.o file.o ui.o file_view.o utf8.o
EXENAME = hund
TESTEXENAME = test/testme
TESTSCRIPTNAME = test/test.sh

all : $(EXENAME)

$(EXENAME) : $(addprefix $(OBJDIR)/, $(OBJ))
	$(LD) $^ -o $(EXENAME) $(LIBS)

$(OBJDIR)/main.o : src/main.c src/include/ui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/utf8.o : src/utf8.c src/include/utf8.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/path.o : src/path.c src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file.o : src/file.c src/include/file.h src/include/path.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/file_view.o : src/file_view.c src/include/file_view.h src/include/file.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ui.o : src/ui.c src/include/ui.h src/include/file_view.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) :
	mkdir $(OBJDIR)

$(OBJDIR)/test.o : test/test.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

test : $(OBJDIR)/test.o $(addprefix $(OBJDIR)/, $(subst main.o,,$(OBJ)))
	$(CC) $(LIBS) -o $(TESTEXENAME) $^ && ./$(TESTEXENAME) && make $(EXENAME) && ./$(TESTSCRIPTNAME)

.PHONY : clean test
clean :
	rm -r testdir &> /dev/null || true
	rm -r $(OBJDIR) &> /dev/null || true
	rm $(EXENAME) $(TESTEXENAME) &> /dev/null || true
