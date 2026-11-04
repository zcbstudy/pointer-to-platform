from ultralytics import YOLO
import cv2
model = YOLO("/home/zcb/yolo11n.pt")
print("模型加载成功")
print("类别数量",len(model.names))
print("0号类别",model.names[0])
image = cv2.imread("/home/zcb/RoadToEmbedded/Jetson/Demo1_Camera/first.jpg")
results = model("/home/zcb/RoadToEmbedded/Jetson/Demo1_Camera/first.jpg")
print("检测框数量",len(results[0].boxes))
boxes = results[0].boxes
for i,box in enumerate(results[0].boxes):
	cls_id = int(box.cls[0].item())
	name = model.names[cls_id]
	conf = float(box.conf[0].item())
	xyxy = box.xyxy[0].tolist()
	
	print("检测框序号",i)
	print("类型编号",cls_id)
	print("类型名称",name)
	print("置信度",conf)
	print("坐标",xyxy)
for i in range(len(boxes)):
	xyxy = boxes[i].xyxy[0].tolist()
	cls_id = int(boxes.cls[0].item())
	name = model.names[cls_id]
	conf = float(boxes.conf[0].item())
	xyxy = boxes.xyxy[0].tolist()
	lable = f"{name}{conf:.2f}"

	xi = int(xyxy[0])
	yi = int(xyxy[1])
	xj = int(xyxy[2])
	yj = int(xyxy[3])
	
	cv2.rectangle(
		image,
		(xi,yi),
		(xj,yj),
		(0,255,0),
		2
	)
	cv2.putText(
		image,
		lable,
		(xi,max(yi-20,20)),
		cv2.FONT_HERSHEY_SIMPLEX,
		0.6,
		(0,255,0),
		2
	)
cv2.imwrite("first_box.jpg",image)
