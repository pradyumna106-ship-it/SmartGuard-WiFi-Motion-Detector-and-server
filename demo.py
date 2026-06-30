import cv2

cap = cv2.VideoCapture("http://172.20.6.33:8080/video")

print("Opened:", cap.isOpened())

while True:
    ret, frame = cap.read()
    print(ret)

    if ret:
        cv2.imshow("Camera", frame)

    if cv2.waitKey(1) == 27:
        break

cap.release()
cv2.destroyAllWindows()