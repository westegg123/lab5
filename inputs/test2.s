.text

mov X1, 0x1000
lsl X1, X1, 16
mov X12, 2
stur W12, [X1, 0x10]
HLT 0
