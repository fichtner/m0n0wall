#!/bin/sh

# write our PID to file
echo $$ > $1

# execute sntp in endless loop; restart if it
# exits (wait 1 second to avoid restarting too fast in case
# the network is not yet setup)
while true; do
	/usr/sbin/sntp -r -P no -l $2 -x $3 $4
	sleep 1
done
