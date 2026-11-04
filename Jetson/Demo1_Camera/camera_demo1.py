import cv2
cap = cv2.VideoCapture(0,cv2.CAP_V4L2)
if cap.isOpened():
	ret,frame = cap.read()
	if ret:
		print("读取成功",ret)
		print("图像尺寸",frame.shape)
		print("左上角像素",frame[0,0])
	else:
		print("读取失败")
else:
		print("摄像头打开失败")
cap.release()
