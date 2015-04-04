#!/bin/sh

set -x

./spreadSink -c ./redis_start.rc | redis-cli > /dev/null

