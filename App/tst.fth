
pwd
load lib.fth 
load classes.fth 
load app_cfg.fth 
load local_cfgapp.fth

variable idx
0 idx !

-1 value param

20 constant #slots
#slots cells 2* value /param

: inc-index
    1 idx +!
;

: mk-index ( n --- offset )
    2* cells
;

: get-param ( n --- 2:obj )
    mk-index param + 2@
;

: set-param ( 2:obj n -- )
    mk-index param + 2!
;

: get-value ( n -- value )
    get-param --> get
;

: set-value ( data n -- )
    get-param --> set
;

: get-index ( -- n )
    idx @
;

\ e.g.
\ -1 vlaue fred
\ 
\ c-string mk-param to fred
\
: mk-param ( -- 2:obj )
    --> alloc   \ 2:obj
    2dup
    --> init    \ 2:obj
    2dup 
    get-index set-param
    inc-index
    
;


/param allocate abort" Allocate failed" to param
param /param erase

c-string --> instance fred 2dup --> init 0 mk-index param + 2!

\ c" fred" find nip 0= [if] c-string --> new fred .( done ) cr [then]
\ c" fred" find nip 0= [if] c-string --> new fred .( done ) cr [else] .( exists ) cr [then]
