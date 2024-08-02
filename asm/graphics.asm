
init:

# Initialize the display
lda 0x03
sta 0xA000

# Base address of display
ldh 0xA0
ldl 0x00

start:
  lda l
  cmp 0xFF
  jpz [end]

  add 0x01
  ldl a
  and 0x0F

  # Event
  jpz [storeA]
  jmp [storeB]

storeA:
  lda 0xAA
  stax
  jmp [start]

storeB:
  lda 0xBB
  stax
  jmp [start]


end:
  
  # sync 
  lda 0x02
  sta 0xA000
  lda 0x02
  sta 0xA000

  hlt

