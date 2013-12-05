
.( Loading boolean.fth ) cr

object --> sub c-boolean
c-byte obj: .state
: init { 2:this -- }
    this --> super --> init
    0 this --> .state --> set
;

: on { 2:this -- }
    0xff this --> .state --> set
;

: off { 2:this -- }
    0 this --> .state --> set
;

: get { 2:this -- flag }
    this --> .state --> get
;

: type { 2:this -- }
    this --> .state --> get
    if
        ." true"
    else
        ." false"
    then
;

end-class


    
