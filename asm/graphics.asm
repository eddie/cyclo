
init:

# Initialize the display
lda 0x03
sta 0xA000

lda 0xA000
ldb 0x0000

start:

  # Done yet?
  cmp 0xFF
  jpz [end]
  add 0x01

  # Even / odd
  push a
  and 0x0F

  # Event
  jpz [storeA]
  jmp [storeB]

storeA:
  pop a 
  ldb 0xAA
  stb [a]
  jmp [start]

storeB:
  pop a 
  ldb 0xBB
  stb [a]
  jmp [start]


end:
  
  # sync 
  lda 0x02
  sta 0xA000
  lda 0x02
  sta 0xA000

  hlt

