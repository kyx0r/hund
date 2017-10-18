#!/bin/bash
exe="hund"
mkdir "testdir"
echo "Starting program test"

# quit
echo "qq" | ./$exe || ( echo "Command qq failed"; exit 1 )

# create directory named 'lol' and exit
echo -e "mklol\rqq" | ./$exe --chdir "testdir"
if [ ! -d "testdir/lol" ]; then
	echo "Command mk failed"
	exit 1
else
	rmdir "testdir/lol"
fi

# move 'testdir/a/a.file' to 'testdir/b.file'
mkdir "testdir/a"
touch "testdir/a/a.file"
echo -e "\tu\tmvb.file\rqq" | ./$exe --chdir "testdir/a"
if [ ! -e "testdir/b.file" ] || [ -e "testdir/a/a.file" ]; then
	echo "Command mv failed"
	exit 1
fi
rm -r "testdir"
mkdir "testdir"

# ...

echo "Finished program test"
rm -r "testdir"
