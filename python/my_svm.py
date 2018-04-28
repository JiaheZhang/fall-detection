from sklearn import svm
import numpy as np
import matplotlib.pyplot as plt

x = np.empty((0, 2))
y = np.array([]) # 分类标记

f = open('data.txt', 'r')
while True:
    str = f.readline()
    if str:
        str_list = str.split()
        aspect_ratio, angle, lable = float(str_list[0]), int(str_list[1]), int(str_list[2])

        x = np.vstack((x, [aspect_ratio, angle]))
        y = np.hstack((y, lable))
    else:
        break

f.close()

clf = svm.SVC(kernel='linear')

clf.fit(x, y)
# print('decision_function:\n', clf.decision_function(x))


# get the separating hyperplane
w = clf.coef_[0]
a = -w[0] / w[1]
xx = np.linspace(0, 8)
yy = a * xx - (clf.intercept_[0]) / w[1]
print(clf.coef_)
print(clf.intercept_)

# plot the parallels to the separating hyperplane that pass through the
# support vectors
b = clf.support_vectors_[0]
yy_down = a * xx + (b[1] - a * b[0])
b = clf.support_vectors_[-1]
yy_up = a * xx + (b[1] - a * b[0])

# plot the line, the points, and the nearest vectors to the plane
plt.plot(xx, yy, 'k-')
# plt.plot(xx, yy_down, 'k--')
# plt.plot(xx, yy_up, 'k--')

plt.title('figure')
plt.xlabel("aspect_ratio")
plt.ylabel('angle')
plt.xlim(xmax=6, xmin=0)
plt.ylim(ymax=91, ymin=0)
plt.scatter(x[:, 0], x[:, 1], c=y)
plt.show()