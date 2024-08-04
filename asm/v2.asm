
  .org 0xff

dat: db 0, "hello",0 ,0

reset: ld a,a

# This is a comment
some_label: 
  ld a,b
  ld b,a
  ld h,l
  ld l,h
  ld l,0xff
  add a
  add l
  add 0x22
  jnz [some_label]
  hlt


##  0xff: 0xff
