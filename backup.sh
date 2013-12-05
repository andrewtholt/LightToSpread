#!/bin/bash

# set -x

LOC="${HOME}/Backup"

if [ ! -d $LOC ]; then
    mkdir -p $LOC
fi

NOW=`date +%Y%m%d`
PWD=`pwd`
NAME=`basename $PWD`
HERE=`dirname $PWD`

BACKUPNAME="${NAME}${NOW}.tgz"

echo "Backup $NAME to $BACKUPNAME"
cd $HERE
tar zcf ${LOC}/${BACKUPNAME} ${NAME}
