#!/bin/bash

if [[ ! -e tglog.pid ]]
then
    echo "tglog is not running!"
    exit 1
fi

p=$(cat tglog.pid)
if ps -p "${p}" > /dev/null
then
    echo "$p is running!"
    exit 2
fi

./stop.sh
sleep 2
./start.sh
