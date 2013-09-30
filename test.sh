#!/bin/sh

infile="./test/in"
outfile="./test/out"

cat /dev/null > $outfile

exec swt -i $infile -o $outfile &
SWTPID=$!

expected="done"

tail -f "$outfile" | while IFS= read -r line
do
    unset response
    response=$line

    if [ "${line}" == "${expected}" ]
    then
        echo "+"
        break
    fi
done &

echo "QUIT" > $infile

wait $SWTPID

