This project started out as an assembler and emulator for an imaginary 8bit CPU, inspired from the book Code by Charles Petzold. The CPU has an instruction width and bus width of 8 bits, and an address space of 16 bits, the instruction set was minimal, and the machine only had a single 8 bit accumulator, no registers.

After restarting work on the project in 2024, and realising the time that goes into designing (even a limited!) 8 bit instruction set, I decided to borrow heavily from the infamous 8080 instruction set. The assembly language different, but the ultimate goal is 8080 compatibility.

Beware! This is mostly a place to learn and improve my C skills whilst scratching an itch!

# TODO

- [ ] Better parse errors, e.g incorrect operand
- [ ] Improved debug messaging and levels
- [ ] Finish full 8080 instruction set.
- [ ] Parser should verify label usage e.g lxi b,[jmpaddr] to throw
- [ ] DB / EQU
- [ ] Stack operations
- [ ] Clear flag operations (CP, CNC, CNZ etc)
- [ ] SHLD Store HL direct
- [ ] LHLD Load HL Direct
- [ ] XCHG Exchange HL with DE / XTHL too
- [ ] DB better handling
- [ ] PUSH PSW POP PSW
- [ ] PUSH / POP
- [ ] Update all status bits and flags where appropriate
- [ ] Cleanup logging and debug levels
