#!/usr/bin/env python3
"""Frame arrival jitter: arrival.py <rtsp-url> [seconds=15] -> fps, gap percentiles, count of gaps > 150 ms."""
import os, sys, time, numpy as np
os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;" + os.environ.get("RTSP_TRANSPORT", "tcp")
import cv2
url = sys.argv[1]; sec = float(sys.argv[2]) if len(sys.argv) > 2 else 15
cap = cv2.VideoCapture(url, cv2.CAP_FFMPEG)
t = []; end = time.time() + sec
while time.time() < end:
    ok, _ = cap.read()
    if ok: t.append(time.time())
g = np.diff(t) * 1000
print(f"{len(t)/sec:5.1f} fps  gap ms: median {np.median(g):.0f} p95 {np.percentile(g,95):.0f} max {g.max():.0f}  >150ms: {(g>150).sum()}  >300ms: {(g>300).sum()}")
