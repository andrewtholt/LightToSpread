-1 constant on
0 constant off
0 value delay
0 value action
0 value fini

: now 1 to delay ;
: immediately 0 to delay ;

-1 value hostname

32 allocate abort" Allocate failed" to hostname
hostname 32 erase

: minutes
    delay 60 * to delay
;

: minute
    minutes
;

: hours 
    delay 3600 * to delay
;

: hour hours ;

: seconds ;
: second ;

: find&exec
    bl word find 0= if 
\        count type ." : Unknown" cr 
        drop -1 to fini
    else
        execute
    then
;

: act
    hostname strlen ?dup 0= if
        ." No host set" 
    else 
        ." Hostname: "
        hostname swap type 
    then
    cr
    action if ." Switch on" else ." switch off" then cr
    ." In " delay . ." Seconds" cr
;

: get_hostname
    bl word dup c@ 0= if
        ." No hostname." cr
    else
        count hostname 32 erase
        hostname swap move
    then
;

: get_delay
    fini 0= if
        0 to delay
        bl word dup c@ if
            count ?number if to delay then
        then
    then
;

: get_sugar
    bl word c@ 0= to fini
;

: get_units
    bl word dup c@ 0<> if
        find&exec
    then
;

: ^power
    0 to fini

    find&exec to action
    get_hostname
    get_sugar
    get_delay

    fini 0= if
        find&exec
    then

    act
;

\ e.g. ^power on compaq in 2 minutes
\      ^power on compaq              -- Apply power now
\      ^power off compaq now         -- Remove power now
\      ^power off compaq immediately -- Remove power now
\      ^power on compaq in 120       -- Apply power in 120 seconds
\      ^power on compaq in 2 minutes -- Apply power in 120 seconds
