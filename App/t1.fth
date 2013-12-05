
only forth also oop definitions seal


20 2* cells constant #array

#array allocate abort" fail" value array
array #array erase



object subclass c-data
    c-string obj: .key
    c-string obj: .value

    : dump { 2:this -- }
        this --> .key   --> type cr
        this --> .value --> type cr
    ;

    : get-key { 2:this -- addr n }
        this --> .key --> get
    ;

    : get-value { 2:this -- addr n }
        this --> .value --> get
    ;

end-class

c-data --> new user
user array 2!

s" user" user --> .key   --> set
s" lite" user --> .value --> set

array 32 dump


