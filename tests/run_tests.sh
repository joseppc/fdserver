#!/bin/bash

../fdserver &>/dev/null &

# give time for the server to start
sleep 1

./fdserver_api 2>/dev/null #| grep -e '^\(FAIL\|PASS\)'