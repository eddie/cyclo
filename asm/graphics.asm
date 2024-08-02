
init:

# Initialize the display
lda 0x03
sta 0xA000

lda 0xA000
ldb 0x0000

start:


  add 0x01

  # Write the value in B to the address in A
  stb [a]
  incb

  cmp 0x000A

  jpz [end]
  jmp [start]


end:
  lda 0x0A
  hlt

