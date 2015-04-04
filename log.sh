#!/bin/sh

OUT=/tmp/log.txt

if [ ! -f $OUT ]; then
    touch $OUT
fi

date >> $OUT
KEY=$1
VALUE=$2

redis-cli set $KEY $VALUE EX 130 > /dev/null
# printf "it\nworked\nreally!\n"
exit 0;

