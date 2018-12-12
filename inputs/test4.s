.text
mov x1, 0x1000
mov x2, 2
lsl x2, x2, 13
lsl x1, x1, 16
mov x11, 2


mul x3, x2, x11
add x11, x11, 1

mul x4, x2, x11
add x11, x11, 1

mul x5, x2, x11
add x11, x11, 1

mul x6, x2, x11
add x11, x11, 1

mul x7, x2, x11
add x11, x11, 1

mul x8, x2, x11
add x11, x11, 1

mul x9, x2, x11
add x11, x11, 1

mul x10, x2, x11
add x11, x11, 1

add x2, x2, x1
add x3, x3, x1
add x4, x4, x1
add x5, x5, x1
add x6, x6, x1
add x7, x7, x1
add x8, x8, x1
add x9, x9, x1


stur x3, [x2, 0x0]
stur x4, [x2, 0x4]
stur x5, [x2, 0x8]
stur x6, [x2, 0xC]
stur X10, [x3, 0x0]
stur X10, [x4, 0x0]
stur X10, [x5, 0x0]
stur X10, [x6, 0x0]
stur X10, [x7, 0x0]
stur X10, [x8, 0x0]
stur X10, [x9, 0x0]
stur X10, [x10, 0x0]
stur x3, [x2, 0x0]
stur X10, [x3, 0x0]
stur X10, [x4, 0x0]
stur X10, [x5, 0x0]

hlt 0
