
export PATH=$PATH:/home/jskim/work/src/github.com/jskim072/Simulator/SCM
export PATH=$PATH:/home/jskim/work/src/github.com/jskim072/Simulator/SCM/trace

filename=`basename $1 .log`
#filename2=${filename}.log
#FIU_parser -o trace/$filename2 -f $1 >> /dev/null 2>&1 &

trace/FIU_parser -o test.txt -f $1 >> /dev/null 2>&1 &

sleep 5

./SCM &

sleep 5

gnuplot -persist << PLOT

set terminal x11
set xlabel "Virtual time"
set ylabel "Logical block address"
plot "other.txt" lt rgb "green" title "OTHERS","seq.txt" lt rgb "red" title "SEQ","loop.txt" lt rgb "blue" title "LOOP"

while(1) {
	replot
	pause 1
}

PLOT

