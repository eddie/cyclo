

  .org 0xff

dat: db 0, "hello",0 ,0

# This is a comment
some_label: ld a,b
       add a,0xff
       add c,[b]
       jnz [some_label]


##  0xff: 0xff
