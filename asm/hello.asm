# Helloworld in cyclo assembly

.org 0000h

lda 0x00
start:
  add 0x01
  jpc [end]
  jmp [start]
end:
  hlt
