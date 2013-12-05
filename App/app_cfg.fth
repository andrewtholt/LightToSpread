only forth also oop definitions

.( Loading app_cfg.fth ) cr

: getFiclParam
    --> get-value
;

: getFiclBoolean ( c-addr -- t|f ) 
    --> get-value s" true" compare 0=
;

: bye
    s" false" INTERACTIVE --> set-value
;

