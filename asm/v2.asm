
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
  cmp b
  cmp h
  cmp 0xff
  add l
  add 0x22
  adc 0x99
  sub 0xff
  xor 0xff
  xor h
  or a 
  dec a 
  dec b
  inc l
  or 0x22
  sub h
  jnz [some_label]

  hlt


##  0xff: 0xff
