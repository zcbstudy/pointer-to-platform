import cv2
cap = cv2.VideoCapture(0,cv2.CAP_V4L2)
if not cap.isOpened():
	raise RuntimeError("摄像头打开失败")
ret,frame = cap.read()
cap.release()
if not ret or frame is None:
	raise RuntimeError("图像保存失败")
print("图像形状",frame.shape)
if frame.ndim != 3 or frame.shape[2] != 3:
	raise RuntimeError("不是预期的三通道图像")
ok = cv2.imwrite("first.jpg",frame)
print ("JEPG图像保存成功")
