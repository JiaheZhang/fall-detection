from PIL import Image
import numpy as np
import serial


# a b 的顺序不能变
def int_to_rgb(fir, sec):
    r = (fir & 0xf8)
    g = ((fir & 0x07) << 5) | ((sec & 0xe0) >> 3)
    b = (sec & 0x1f) << 3
    return r, g, b


# 帧同步信号
sync = [255, 0, 255, 0, 255, 0]
sync_now = [i for i in range(0, 6)]
is_sync = False

data = np.zeros(240*320*3, dtype='int8')
data = data.reshape(320, 240, 3)
i, j = 0, 0

ser = serial.Serial("com3", baudrate=115200)
cnt = -1
is_save = False
fir_or_sec = True

while True:
    x = ser.read(1)
    get_value = int.from_bytes(x, byteorder='big')
    fir_or_sec = not fir_or_sec
    sync_now.append(get_value)
    sync_now = sync_now[1:]
    if sync_now == sync:

        if cnt >= 0:
            img = Image.fromarray(data, 'RGB')
            img.save("{}.png".format(cnt))
            # img.show()

        # 帧数+1
        cnt = cnt + 1
        is_save = True
        is_sync = True
        fir_or_sec = True
        i, j = 0, 0
        continue

    if is_save:
        if fir_or_sec:
            R, G, B = int_to_rgb(sync_now[4], sync_now[5])

            print(i, j, R, G, B)
            if i < 239 and j < 319:
                data[i][j][0], data[i][j][1], data[i][j][2] = R, G, B

            j = j + 1
            if j == 240:
                i, j = i + 1, 0









'''
# 生成一个数组，维度为100*100，灰度值一定比255大
n_array = np.array([range(10000)], dtype='int')
n_array = n_array.reshape([100, 100])
# 调用Image库，数组归一化
img = Image.fromarray(n_array*255.0/9999)
# 转换成灰度图
# img=img.covert('L')
# 可以调用Image库下的函数了，比如show()
img.show()
# Image类返回矩阵的操作
img_data = np.matrix(img.getdata(), dtype='float')
img_data = img_data.reshape(n_array.shape[0], n_array.shape[1])
# 图像归一化，生成矩阵
n_matrix = img_data*9999/255.0
'''

