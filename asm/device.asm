
ldi 0x40

start:
add 0x01
stm 0xA000
cmp 0x5A
jpz [end]
jmp [start]

end:
# New line
ldi 0x0A
stm 0xA000
hlt