#!/bin/sh
#crude but eventually i'll see a something out of it

BIN="../swt"
IN="./in"
OUT="./out"

exec $BIN -i $IN -o $OUT &

SWT_PID=$!

cat /dev/null > $OUT

echo "window testing This is a new window" > $IN
sleep 1
WIN_ID=`grep "window testing" $OUT | awk '{print $3}'`
echo "WIN_ID=${WIN_ID}"
sleep 1
xdotool keydown --window $WIN_ID ctrl+c

echo "window testing2 This is a new window" > $IN
sleep 1
WIN_ID=`grep "window testing2" $OUT | awk '{print $3}'`
echo "WIN_ID=${WIN_ID}"
sleep 1
xdotool keydown --window $WIN_ID ctrl+q
