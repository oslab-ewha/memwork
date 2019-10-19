ps aux | grep -i $1 | awk {'print $2'} | xargs kill -9
