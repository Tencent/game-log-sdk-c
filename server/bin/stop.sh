#!/bin/bash
if [[ ! -e tglog.pid ]]
then
    echo "tglog is not running!"
    exit 1
fi

# shellcheck disable=SC2002
cat tglog.pid | xargs kill -s SIGTERM
rm tglog.pid
