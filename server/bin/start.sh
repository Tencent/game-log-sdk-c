#!/bin/bash

cd $(dirname $0)

if [[ -e tglog.pid ]]
then
   p=$(cat tglog.pid)
   #echo $p
   if ps -p "${p}" > /dev/null
   then
      echo "$p is running!"
      exit 1
   fi
fi

nohup ${PWD}/tglog serve --config "../cfg/simplest.yaml" > tglog.log 2>&1 &
p=$!
echo $p > tglog.pid

sleep 3
cat tglog.log

echo ""
echo "pid is $p"
