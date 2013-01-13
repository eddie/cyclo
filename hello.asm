# Helloworld in cyclo assembly

.org 0000h

start:
  ldi 0x00
  add 0x0A
  jpc end
  jmp start
end:
  hlt
  ldi [0x00]
