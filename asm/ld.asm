

  lda b
  ldb a 
  lda [0x00]
  ldb [0x00]

  lda [0x00] # load with value from address 0x0

  lda 0xff
  ldb 0xff

  hlt
