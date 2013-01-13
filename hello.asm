# Helloworld in cyclo assembly

.org 0000h

ldi 0x00
start:
  add 0x0A
  jpc [end]
  jmp [start]
end:
  hlt
