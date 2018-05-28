#!/bin/bash

NEW_PATH=$(mktemp -p "" -u fdserver_socket.XXXX)
echo "path: $NEW_PATH"

../src/fdserver -p ${NEW_PATH} &>/dev/null &

# give time for the server to start
sleep 1

./fdserver_api -p ${NEW_PATH} 2>/dev/null #| grep -e '^\(FAIL\|PASS\)'
retval=$?

killall -HUP fdserver
wait

rm ${NEW_PATH}

exit $retval
