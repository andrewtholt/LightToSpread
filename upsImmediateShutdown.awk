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
                if(online == "true") {
                    print "Power OK"
                }
                cmd=sprintf("redis-cli set ONLINE %s EX 130 > /dev/null", online)
                system( cmd )
            }

/ONBATT/    {
                onbatt=$2;
                if (onbatt == "true" ) {
                    print "Shutdown now !!!"
                }   
                cmd=sprintf("redis-cli set ONBATT %s EX 130 > /dev/null", onbatt)
                system( cmd )
            }
/BATTLEVEL/ {
                battlevel=$2
                print "Battery level is ", battlevel
                cmd=sprintf("redis-cli set BATTLEVEL %s EX 130 > /dev/null", battlevel)
                system( cmd )
            }
/RUNTIME/   {
                runtime=$2
                print "Runtime is ",runtime," Minutes"
                cmd=sprintf("redis-cli set RUNTIME %s EX 130 > /dev/null", runtime)
                system( cmd )
            }
