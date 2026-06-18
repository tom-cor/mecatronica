# CameraWebServertuneado — Quick Start

Checklist
- [x] Create README with virtual environment activation steps
- [x] Include installing dependencies from `requirements.txt`
- [x] Show how to run the app using the requested IPs (camera: `192.168.0.101`, robot ESP32: `192.168.0.102`)

## Overview
This project provides a simple OpenCV-based tracker that reads an MJPEG stream from a camera and sends movement commands to an ESP32 robot controller.

The tracker script is `opencvscript.py`. It expects two positional arguments: the camera IP and the robot IP. The camera stream URL used by the script is `http://<camera_ip>:81/stream` and the robot endpoint is `http://<robot_ip>/move`.

## Prerequisites
- Linux (instructions use bash)
- Python 3.8+ with `venv` module available
- Network access to the camera and the ESP32 robot

Recommended system packages for OpenCV on many Linux distributions (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y libgl1-mesa-glx libglib2.0-0 libsm6 libxrender1 libxext6
```

## Setup (create & activate virtual environment)
Create and activate a virtual environment in the project root:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Upgrade pip (optional but recommended):

```bash
python -m pip install --upgrade pip
```

## Install Python dependencies
Install dependencies from the provided `requirements.txt`:

```bash
pip install -r requirements.txt
```

The `requirements.txt` in this repo includes:
- `opencv-python`
- `numpy`
- `requests`

If `pip install` fails for `opencv-python`, ensure the system packages in the Prerequisites section are installed and try again.

## Run the tracker (example using your IPs)
Start the tracker with the camera at `192.168.0.101` and the robot at `192.168.0.102`:

```bash
source .venv/bin/activate   # if not already active
python3 opencvscript.py 192.168.0.101 192.168.0.102
```

What to expect:
- The script will attempt to open `http://192.168.0.101:81/stream` and display a window named "Video Stream Tracking".
- Use the trackbars in the window to tune the target box and center.
- The script will send HTTP POST commands to `http://192.168.0.102/move` with JSON payloads like `{ "command": "forward" }`.
- Press `q` in the video window to quit.

## Quick troubleshooting
- If the script prints "Could not open video stream", verify the camera is reachable:

```bash
curl -v http://192.168.0.101:81/stream
```

- If POST commands are not arriving at the robot, test its endpoint:

```bash
curl -X POST http://192.168.0.102/move -H "Content-Type: application/json" -d '{"command":"stop"}'
```

- Make sure your PC and both devices are on the same LAN and firewalls are not blocking ports.

## Stop / cleanup
Deactivate the virtual environment when finished:

```bash
deactivate
```

## Files referenced
- `opencvscript.py` — main tracker script
- `requirements.txt` — Python dependencies list

## Requirements coverage
- Virtual environment + activation: Done
- Install dependencies: Done (instructions use `requirements.txt`)
- Run app with camera `192.168.0.101` and robot `192.168.0.102`: Done (example run command)

If you want, I can also add a small systemd service or a shell-run script to automate startup; tell me which you prefer.
