# fall-detection
my graduation project. fall detection of the old in video
本科的毕业设计
基于smt32的家庭报警器的设计
使用c\c++作为编程语言，在stm32单片机上运行


使用ov7725彩色摄像头拍摄视频，实时检测视野中是否有人进入，是否有人摔倒
若有人摔倒，则处罚报警，向远程端发送


步骤：
1.背景差分提取人体
2.选中人体的标定框
3.提取人体标定框的宽高比、人体结构的倾斜姿态（使用协方差矩阵）
4.用SVM分类器进行分类，判定是否摔倒
