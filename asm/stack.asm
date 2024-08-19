
  lxi b, 0xabcd

  push b
  pop d
  push d
  pop h


  ld a,0x10

  push psw
  ld a,0x20
  cmp 0x20
  add 0x256
  push psw
  pop psw
  pop psw


  lhld 0x00

