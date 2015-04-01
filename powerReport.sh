#!/bin/bash

set -x

ONBATT=`redis-cli get ONBATT`

if [ "$ONBATT" == "" ]; then
    echo "No UPS data."
    exit 0
fi

if [ "$ONBATT" == "true" ]; then
    echo "Running on battery."
fi


if [ "$ONBATT" == "false" ]; then
    echo "Running on line"
fi
