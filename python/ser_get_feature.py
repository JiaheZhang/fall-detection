import serial
import time

# 获取当前时间 用于创建文件
localtime = time.strftime("%Y-%m-%d,%H.%M.%S", time.localtime())
ser = serial.Serial('com3', 9600)
cnt = 0

f = open('{}.txt'.format(localtime), 'w+')

while True:
    x = ser.read(1)
    y = ser.read(1)
    z = ser.read(1)
    a = int.from_bytes(x, byteorder='big')
    b = int.from_bytes(y, byteorder='big')
    c = int.from_bytes(z, byteorder='big')
    if a != 0 and b != 0:
        print(cnt, a / b, c, 0)
        f.write(str(cnt) + ' ' + str(a/b) + ' ' + str(c) + ' 0' + '\n')

    # 视情况而定
    if cnt == 300:
        break

    cnt = cnt + 1

f.close()