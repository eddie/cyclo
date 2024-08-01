

  lda b
  ldb a 
  lda [0x19]
  ldb [0x91]

  lda [0x00] # load with value from address 0x0

  lda 0x80
  ldb 0x08

  hlt
