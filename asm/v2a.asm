  ld a,0x02
  add a
  ld b,0x05
  add b
  inc a 
  inc b
  dec a 
  dec b

  ld a,0x01
  or 0x01

  lxi H,0x01234
  lxi SP, 0x1
  # Stpre 0x25 at memory location 0xFF
  ld a,0x25
  lxi H,0xff
  # Self modifying!
  sta 0x00
  stax B

