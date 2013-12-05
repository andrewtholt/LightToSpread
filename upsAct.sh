#!/bin/sh

set -x

STATUS=0;
ONBATT=`redis-cli get ONBATT | sed 's/\"//g'`

if [ "${ONBATT}" == "true" ]; then
    echo "Shutdown now."
    STATUS=1
fi

exit $STATUS

