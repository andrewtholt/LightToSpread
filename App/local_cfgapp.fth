
only forth also oop definitions seal

.( Loading local_appcfg.fth ) cr

\ : pdump
\ cr 09 emit ." Parameters" cr

\ ." User         :" user --> type cr
\ ." Group        :" group --> type cr
\ ." Default group:" defaultGroup --> type cr
\ ." Mode         :" mode --> type cr
\ ." Autoconnect  :" autoconnect --> type cr
\ ." App Dir      :" appDir --> type cr
\ ." Config File  :" configFile --> type cr
\ ." Me           :" me --> type cr
\ ." Debug        :" debug --> type cr

\ cr
\ ;

: ^fred
    cr ." Hello from fred" cr
;


