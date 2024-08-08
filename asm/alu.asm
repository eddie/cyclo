

init:
  jmp [start]


start:

  ld a,0x01
  and 0x01

  # Zero flag set if they are equal
  ld a,0xff
  cmp 0xff

  # Zero flag reset if not equal
  cmp 0x00


  ld a,0xff
  and 0x00
