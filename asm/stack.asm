# Offset to output region
ldh 0xA0
ldl 0x01

lda 0x60
ldb 0x90

push a 
push b

pop a 
stax
incl 
pop a
stax

#sync 
lda 0x02
sta 0xA000





  
