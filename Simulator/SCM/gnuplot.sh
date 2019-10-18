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

