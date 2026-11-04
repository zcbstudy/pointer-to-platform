import cv2
from ultralytics import YOLO

model = YOLO("/home/zcb/yolo26s.pt")

cap = cv2.VideoCapture(0,cv2.CAP_V4L2)
if not cap.isOpened():
	raise RuntimeError("摄像头打开失败")

while True:
	
	ret,frame = cap.read()
	if not ret:
		raise RuntimeError("读取失败",ret)
		break
	results = model(frame)
	boxes = results[0].boxes
	
	for i in range(len(boxes)):
		xyxy = boxes[i].xyxy[0].tolist()
		cls_id = int(boxes[i].cls[0].item())
		name = model.names[cls_id]
		conf = float(boxes[i].conf[0].item())
		lable = f"{name}{conf:.2f}"

		xi = int(xyxy[0])
		yi = int(xyxy[1])
		xj = int(xyxy[2])
		yj = int(xyxy[3])

		cv2.rectangle(
			frame,
			(xi,yi),
			(xj,yj),
			(0,255,0),
			2
		)
		
		cv2.putText(
			frame,
			lable,
			(xi,max(yi-20,20)),
			cv2.FONT_HERSHEY_SIMPLEX,
			0.6,
			(0,255,0),
			2
		)

	cv2.imshow("Jetson Carema",frame)
	key = cv2.waitKey(1)
	if key == ord('q'):
		break
cap.release()
cv2.destroyAllWindows()
