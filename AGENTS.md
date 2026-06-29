# AGENTS.md

## Project Structure
- `proyecto-final/CameraWebServertuneado/opencvscript.py` — Main tracking script
- `proyecto-final/CameraWebServertuneado/CameraWebServertuneado.ino` — ESP32-CAM firmware
- `proyecto-final/demo-brazo-auto/demo-brazo-auto.ino` — Robot arm + car firmware

## How to Run
1. Flash `CameraWebServertuneado` to ESP32-CAM
2. Flash `demo-brazo-auto` to robot ESP32
3. Run tracker from `CameraWebServertuneado/`:
   `python opencvscript.py <camera_ip> <robot_ip>`

## Architecture
ESP32-CAM → MJPEG stream (port 81) → Python/OpenCV detects yellow objects → HTTP POST `/move` → robot drives + arm sequence

## Tracking State Machine
`IDLE → CENTERING → APPROACHING → FINE_APPROACH → BACKING → STOPPED`

Stopped for 2s → triggers arm: reverse sequence → close gripper → forward sequence

## Key Files
- `CameraWebServertuneado/opencvscript.py:1-239` — Full tracking logic with HSV trackbars
- `demo-brazo-auto/demo-brazo-auto.ino:1-622` — Servo/motor control with web dashboard
- `CameraWebServertuneado/board_config.h:1-34` — Select camera model
