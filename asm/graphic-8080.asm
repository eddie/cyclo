
init:

# Initialize the display
ld a, 0x03
sta 0xA000

# Base address of display
ld b,0xA0
ld c,0x00

start:
  # todo: inx would be helpful!
  # done yet?
  ld a,c 
  cmp 0xFF
  jnz [end]
  
  ld a,c
  sub 0x01
  stax b
  inx b

  jmp [start]

end:
  ld a,0x02
  sta 0xA000
  hlt


