#!/bin/bash
exe="hund"
mkdir "testdir"
echo "Starting program test"

# quit
echo "qq" | ./$exe || ( echo "Command qq failed"; exit 1 )

# create directory named 'lol' and exit
echo -e "mklol\nqq" | ./$exe testdir
if [ ! -d "testdir/lol" ]; then
	echo "Command mk failed"
	exit 1
fi

# remove directory 'lol' and exit
echo -e "remqq" | ./$exe testdir
if [ -d "testdir/lol" ]; then
	echo "Command rm failed"
	rmdir testdir/lol
	exit 1
fi

# move 'testdir/a/a.file' to 'testdir/a.file'
mkdir -p "testdir/a"
touch "testdir/a/a.file"
echo -e "mv\nqq" | ./$exe testdir/a testdir
if [ ! -e "testdir/a.file" ] || [ -e "testdir/a/a.file" ]; then
	echo "Command mv failed"
	exit 1
fi

# ...

echo "Finished program test"
rm -r "testdir"
