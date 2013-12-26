#!/bin/sh
#crude but eventually i'll see a something out of it

BIN="../swt"
IN="./in"
OUT="./out"

#exec valgrind --leak-check=full $BIN -i $IN -o $OUT &
exec $BIN -i $IN -o $OUT &

SWT_PID=$!

cat /dev/null > $OUT

echo "window testing This is a new window" > $IN
sleep 1
WIN_ID=`grep "window testing" $OUT | awk '{print $3}'`
echo "WIN_ID=${WIN_ID}"
sleep 1
echo "add testing text 'lorem ipsum'" > $IN
sleep 2
echo "add testing text 'lorem ipsum'" > $IN
sleep 2
echo "add testing text 'lorem ipsum'" > $IN
sleep 2
echo "add testing text 'lorem ipsum'" > $IN
sleep 2
echo "dump" > $IN
sleep 2
xdotool keydown --window $WIN_ID ctrl+q

