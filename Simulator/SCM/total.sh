
export PATH=$PATH:/home/jskim/work/src/github.com/jskim072/Simulator/SCM
export PATH=$PATH:/home/jskim/work/src/github.com/jskim072/Simulator/SCM/trace

filename=`basename $1 .log`
#filename2=${filename}.log
#FIU_parser -o trace/$filename2 -f $1 >> /dev/null 2>&1 &

trace/FIU_parser -o test.txt -f $1 >> /dev/null 2>&1 &

sleep 5

./simulation3.sh &

./SCM 


