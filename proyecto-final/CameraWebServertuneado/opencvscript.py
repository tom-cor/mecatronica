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

def send_movement(ip, direction, ticks=None):
    """Sends the movement command to the robot endpoint in a background thread."""
    url = f"http://{ip}/move"
    try:
        data = {"command": direction}
        if ticks is not None:
            data["ticks"] = ticks
        requests.post(url, json=data, timeout=1.0)
    except requests.exceptions.RequestException:
        pass

def execute_arm_sequence(ip, arm_in_progress):
    """Opens gripper → reverse → close → 2s → forward → 2s after robot stops."""
    arm_in_progress.set()
    try:
        requests.post(f"http://{ip}/move", json={"command": "stop"}, timeout=2)
        time.sleep(1)
        requests.get(f"http://{ip}/gripper/open", timeout=5)
        time.sleep(2)
        requests.get(f"http://{ip}/reverse", timeout=10)
        time.sleep(5)
        requests.get(f"http://{ip}/gripper/close", timeout=5)
        time.sleep(2)
        requests.get(f"http://{ip}/sequence", timeout=10)
        time.sleep(5)
    except requests.exceptions.RequestException:
        pass
    finally:
        arm_in_progress.clear()

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
    
    cv2.createTrackbar("Target X", "Video Stream Tracking", 320, 640, nothing)
    cv2.createTrackbar("Center Thresh", "Video Stream Tracking", 30, 160, nothing)
    cv2.createTrackbar("Fine Thresh", "Video Stream Tracking", 15, 160, nothing)
    cv2.createTrackbar("Nudge Ticks", "Video Stream Tracking", 5, 50, nothing)
    cv2.createTrackbar("Stop Area", "Video Stream Tracking", 15500, 30000, nothing)
    cv2.createTrackbar("Fine Area", "Video Stream Tracking", 10000, 30000, nothing)
    cv2.createTrackbar("Over Area", "Video Stream Tracking", 17000, 30000, nothing)

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
    nudge_state = "IDLE"
    nudge_start = 0

    abs_dx = 0
    arm_sequence_triggered = False
    stopped_since = None
    arm_in_progress = threading.Event()

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Stream disconnected or ended.")
            break

        frame = cv2.resize(frame, (frame_width, frame_height))
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        target_x = cv2.getTrackbarPos("Target X", "Video Stream Tracking")
        center_thresh = cv2.getTrackbarPos("Center Thresh", "Video Stream Tracking")
        fine_thresh = cv2.getTrackbarPos("Fine Thresh", "Video Stream Tracking")
        stop_area = cv2.getTrackbarPos("Stop Area", "Video Stream Tracking")
        fine_tune_area = cv2.getTrackbarPos("Fine Area", "Video Stream Tracking")
        overshoot_area = cv2.getTrackbarPos("Over Area", "Video Stream Tracking")

        cv2.line(frame, (target_x, 0), (target_x, frame_height), (255, 255, 255), 1)
        cv2.line(frame, (target_x - center_thresh, 0), (target_x - center_thresh, frame_height), (100, 100, 100), 1)
        cv2.line(frame, (target_x + center_thresh, 0), (target_x + center_thresh, frame_height), (100, 100, 100), 1)
        cv2.line(frame, (target_x - fine_thresh, 0), (target_x - fine_thresh, frame_height), (50, 200, 50), 1)
        cv2.line(frame, (target_x + fine_thresh, 0), (target_x + fine_thresh, frame_height), (50, 200, 50), 1)

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

        # --- Two-phase approach state machine ---
        robot_cmd = None
        state = "IDLE"
        obj_area = None

        if contours_yellow:
            largest_yellow = max(contours_yellow, key=cv2.contourArea)
            obj_area = cv2.contourArea(largest_yellow)

            if obj_area > 500:
                x, y, w, h = cv2.boundingRect(largest_yellow)

                obj_cx = x + w // 2
                obj_cy = y + h // 2

                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 255), 2)
                cv2.circle(frame, (obj_cx, obj_cy), 5, (0, 255, 255), -1)
                cv2.line(frame, (target_x, obj_cy), (obj_cx, obj_cy), (0, 255, 255), 1)

                dx = obj_cx - target_x
                abs_dx = abs(dx)

                if obj_area >= overshoot_area:
                    state = "BACKING"
                    robot_cmd = "backward"
                elif obj_area >= stop_area:
                    if abs_dx <= fine_thresh:
                        state = "STOPPED"
                        robot_cmd = "stop"
                    else:
                        state = "CENTERING"
                        robot_cmd = "right" if dx > 0 else "left"
                elif abs_dx > (fine_thresh if obj_area >= fine_tune_area else center_thresh):
                    state = "CENTERING"
                    robot_cmd = "right" if dx > 0 else "left"
                elif obj_area >= fine_tune_area:
                    state = "FINE_APPROACH"
                    robot_cmd = "forward"
                else:
                    state = "APPROACHING"
                    robot_cmd = "forward"

        # Fallback to stop when no object
        if robot_cmd is None:
            robot_cmd = "stop"

        current_time = time.time()
        nudge_ticks = cv2.getTrackbarPos("Nudge Ticks", "Video Stream Tracking")

        # Nudge state machine (Arduino auto-stops after tick target)
        if nudge_state == "IDLE":
            if state in ("CENTERING", "APPROACHING", "FINE_APPROACH", "BACKING"):
                if not arm_in_progress.is_set():
                    threading.Thread(target=send_movement, args=(args.robot_ip, robot_cmd, nudge_ticks), daemon=True).start()
                nudge_state = "COOLDOWN"
                nudge_start = current_time
        elif nudge_state == "COOLDOWN":
            if current_time - nudge_start >= cooldown:
                nudge_state = "IDLE"

        # Safety stop — send stop every cooldown while no object in sight
        if state == "IDLE" and current_time - last_cmd_time > cooldown:
            if not arm_in_progress.is_set():
                threading.Thread(target=send_movement, args=(args.robot_ip, "stop"), daemon=True).start()
            last_cmd_time = current_time

        # Arm sequence trigger — fires after 2s of continuous STOPPED
        if state == "STOPPED":
            if stopped_since is None:
                stopped_since = current_time
            elif not arm_sequence_triggered and not arm_in_progress.is_set() and current_time - stopped_since >= 2.0:
                arm_sequence_triggered = True
                arm_in_progress.set()
                send_movement(args.robot_ip, "stop")
                threading.Thread(target=execute_arm_sequence, args=(args.robot_ip, arm_in_progress), daemon=True).start()
        elif not arm_in_progress.is_set():
            arm_sequence_triggered = False
            stopped_since = None

        # HUD backdrop
        overlay = frame.copy()
        cv2.rectangle(overlay, (5, 12), (635, 112), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.35, frame, 0.65, 0, frame)

        # HUD
        state_colors = {
            "STOPPED": (0, 255, 0),
            "BACKING": (0, 0, 255),
            "FINE_APPROACH": (255, 255, 0),
            "APPROACHING": (255, 0, 0),
            "CENTERING": (0, 165, 255),
            "IDLE": (128, 128, 128),
        }
        color = state_colors.get(state, (255, 255, 255))
        cv2.putText(frame, f"Cam: {args.camera_ip} | Bot: {args.robot_ip}", (10, 22),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
        cv2.putText(frame, f"TargetX:{target_x}  CTh:{center_thresh}  FTh:{fine_thresh}  Ticks:{nudge_ticks}", (10, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
        cv2.putText(frame, f"Stop:{stop_area}  Fine:{fine_tune_area}  Over:{overshoot_area}", (10, 58),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
        if obj_area is not None:
            cv2.putText(frame, f"[{state}]  Area:{int(obj_area)}  Sending:{robot_cmd.upper()}", (10, 80),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

        cv2.imshow("Video Stream Tracking", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
