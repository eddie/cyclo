

lda 0xA000

start:

add 0x01
cmp 0xA005
jpz [end]
jmp [start]


end:
# New line
lda 0x0A
hlt

