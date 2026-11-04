import cv2

cap = cv2.VideoCapture(0, cv2.CAP_V4L2)

if not cap.isOpened():
    print("摄像头打开失败")
    raise SystemExit(1)

while True:
    ret, frame = cap.read()

    if not ret:
        print("图像读取失败")
        break

    cv2.imshow("Jetson Camera", frame)

    key = cv2.waitKey(1)

    if key == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
