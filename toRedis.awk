#!/usr/bin/awk -f
BEGIN {
    online="true";
    onbatt="false";
    lowbatt="false";
    runtime=1000;
    battlevel=200;
}

/ONLINE/    {
                online=$2;

                cmd=sprintf("redis-cli set ONLINE %s EX 130 > /dev/null", online)
                print cmd
                system( cmd )
            }

/ONBATT/    {
                onbatt=$2;
                cmd=sprintf("redis-cli set ONBATT %s EX 130 > /dev/null", onbatt)
                system( cmd )
            }
/BATTLEVEL/ {
                battlevel=$2
                cmd=sprintf("redis-cli set BATTLEVEL %s EX 130 > /dev/null", battlevel)
                system( cmd )
            }
/RUNTIME/   {
                runtime=$2
                cmd=sprintf("redis-cli set RUNTIME %s EX 130 > /dev/null", runtime)
                system( cmd )
            }
