This folder is for the camera calibration feature in "Camera Capture" popup,
which undistort the captured images by using OpenCV's calibration feature.

### How to calibrate camera

1. Prepare the checkerboard

- Print out "checkerboard.tif" to A4 sized paper.
- It would be better to paste the checkerboard pattern on some flat panel like cardboard.

2. Capture the pattern

- In the Camera Capture popup, enable "Camera Calibration" box.
- Click "Start calibration".
- Take 10 snapshots of the checkerboard pattern in various positions and angles.
- Once the capturing is done, calibration will be automatically applied.

3. Export & load calibration settings

- Calibration settings will be saved under this folder with name
  "[MACHINENAME]_[CAMERANAME]_[RESOLUTION].xml"
- The settings file will be overwritten after finishing the new calibration.
- You can export the file for backup, or load it afterwards.
