import cv2
import numpy as np
import argparse
import sys
import requests
import threading
import time

def nothing(x):
    """Dummy callback function for trackbars."""
    pass

def send_movement(ip, direction):
    """Sends the movement command to the robot endpoint in a background thread."""
    url = f"http://{ip}/move"
    try:
        requests.post(url, json={"command": direction}, timeout=1.0)
    except requests.exceptions.RequestException:
        pass

def main():
    parser = argparse.ArgumentParser(description="Track objects and send movement commands.")
    parser.add_argument("camera_ip", help="The IP address of the camera (e.g., 192.168.1.100)")
    parser.add_argument("robot_ip", help="The IP address of the movement controller (e.g., 192.168.1.101)")
    args = parser.parse_args()

    stream_url = f"http://{args.camera_ip}:81/stream"
    print(f"Connecting to camera at {stream_url}...")

    cap = cv2.VideoCapture(stream_url)
    
    if not cap.isOpened():
        print(f"Error: Could not open video stream at {stream_url}")
        sys.exit(1)

    cv2.namedWindow("Video Stream Tracking")
    
    cv2.createTrackbar("X Center", "Video Stream Tracking", 320, 640, nothing)
    cv2.createTrackbar("Y Center", "Video Stream Tracking", 240, 480, nothing)
    cv2.createTrackbar("X Gap", "Video Stream Tracking", 50, 320, nothing)
    cv2.createTrackbar("Y Gap", "Video Stream Tracking", 50, 240, nothing)

    print("--- Tracking Started ---")
    print(f"Sending movement commands to http://{args.robot_ip}/move/")
    print("Press 'q' in the video window to quit.")

    frame_width = 640
    frame_height = 480

    lower_blue = np.array([90, 100, 100])
    upper_blue = np.array([130, 255, 255])
    lower_yellow = np.array([20, 100, 100])
    upper_yellow = np.array([40, 255, 255])

    last_cmd_time = 0
    cooldown = 0.5 

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Stream disconnected or ended.")
            break

        frame = cv2.resize(frame, (frame_width, frame_height))
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        c_x = cv2.getTrackbarPos("X Center", "Video Stream Tracking")
        c_y = cv2.getTrackbarPos("Y Center", "Video Stream Tracking")
        g_x = cv2.getTrackbarPos("X Gap", "Video Stream Tracking")
        g_y = cv2.getTrackbarPos("Y Gap", "Video Stream Tracking")

        cv2.drawMarker(frame, (c_x, c_y), (255, 255, 255), cv2.MARKER_CROSS, 20, 1)
        cv2.rectangle(frame, (c_x - g_x, c_y - g_y), (c_x + g_x, c_y + g_y), (255, 255, 255), 1)

        blue_mask = cv2.inRange(hsv, lower_blue, upper_blue)
        yellow_mask = cv2.inRange(hsv, lower_yellow, upper_yellow)

        kernel = np.ones((5, 5), np.uint8)
        blue_mask = cv2.morphologyEx(blue_mask, cv2.MORPH_OPEN, kernel)
        yellow_mask = cv2.morphologyEx(yellow_mask, cv2.MORPH_OPEN, kernel)

        contours_blue, _ = cv2.findContours(blue_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        contours_yellow, _ = cv2.findContours(yellow_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        for cnt in contours_blue:
            if cv2.contourArea(cnt) > 500:
                x, y, w, h = cv2.boundingRect(cnt)
                cv2.rectangle(frame, (x, y), (x + w, y + h), (255, 0, 0), 2)
                cv2.putText(frame, "Blue", (x, y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)

        if contours_yellow:
            largest_yellow = max(contours_yellow, key=cv2.contourArea)
            
            if cv2.contourArea(largest_yellow) > 500:
                x, y, w, h = cv2.boundingRect(largest_yellow)
                
                obj_cx = x + w // 2
                obj_cy = y + h // 2 

                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 255), 2)
                cv2.circle(frame, (obj_cx, obj_cy), 5, (0, 255, 255), -1)
                cv2.line(frame, (c_x, c_y), (obj_cx, obj_cy), (0, 255, 255), 1)

                dx = obj_cx - c_x
                dy = obj_cy - c_y
                
                x_cmd = ""
                y_cmd = ""
                robot_cmd = None

                if abs(dx) > g_x:
                    x_cmd = "PAN RIGHT ->" if dx > 0 else "<- PAN LEFT"
                if abs(dy) > g_y:
                    y_cmd = "TILT DOWN v" if dy > 0 else "^ TILT UP"

                # Priority logic: Move or Stop
                if abs(dx) > g_x or abs(dy) > g_y:
                    if abs(dx) > abs(dy):
                        robot_cmd = "right" if dx > 0 else "left"
                    else:
                        robot_cmd = "backwards" if dy > 0 else "forward"
                else:
                    # The object is inside the target box
                    robot_cmd = "stop"

                # Dispatch the command if off cooldown
                current_time = time.time()
                if robot_cmd and (current_time - last_cmd_time > cooldown):
                    threading.Thread(target=send_movement, args=(args.robot_ip, robot_cmd), daemon=True).start()
                    last_cmd_time = current_time

                # Update the HUD
                if not x_cmd and not y_cmd:
                    cv2.putText(frame, "[ TARGET ALIGNED ]", (220, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                else:
                    if x_cmd:
                        cv2.putText(frame, x_cmd, (50, 420), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                    if y_cmd:
                        cv2.putText(frame, y_cmd, (50, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                    
                if robot_cmd:
                    # Color code the HUD text: Green for Stop, Orange for Movement
                    cmd_color = (0, 255, 0) if robot_cmd == "stop" else (0, 165, 255)
                    cv2.putText(frame, f"SENDING: {robot_cmd.upper()}", (440, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.6, cmd_color, 2)

        cv2.putText(frame, f"Cam: {args.camera_ip} | Bot: {args.robot_ip}", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
        
        cv2.imshow("Video Stream Tracking", frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
