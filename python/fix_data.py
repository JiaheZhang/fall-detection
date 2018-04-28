f = open('2018-04-28,16.20.56.txt', 'r')
f_data = open('2018-04-28,16.20.56_data.txt', 'w+')


def data_transfer(str_of_three):
    num_list = str_of_three.split()
    return num_list[1:]


while True:
    x = f.readline()
    if x:
        print(x)
        str_temp = data_transfer(x)
        for i in range(0, 2):
            f_data.write(str_temp[i])
            f_data.write(" ")
        f_data.writelines(str_temp[2] + "\n")
    else:
        break

f_data.close()
f.close()
