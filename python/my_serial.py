import serial

ser = serial.Serial('com3', 9600)

angle = []
aspect_ratio = [[]]
cnt = 0

while True:
    x = ser.read(1)
    y = ser.read(1)
    z = ser.read(1)
    a = int.from_bytes(x, byteorder='big')
    b = int.from_bytes(y, byteorder='big')
    c = int.from_bytes(z, byteorder='big')
    aspect_ratio.append([a,b])
    angle.append(c)
    print(cnt,a,b,c)
    cnt = cnt + 1



