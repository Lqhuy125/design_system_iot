"""
How to run: python imu_plot.py --port COM15 --baud 115200


""""
# imu_plot.py
import argparse, re, sys, time, csv
import numpy as np
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# --------- CLI args ----------
parser = argparse.ArgumentParser(description="Real-time IMU plot from serial (Arduino/ESP32)")
parser.add_argument("--port", required=True, help="Serial port, e.g., COM15 or /dev/ttyUSB0")
parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
parser.add_argument("--save_csv", default=None, help="Optional CSV file to log data")
parser.add_argument("--hz", type=float, default=20, help="Plot refresh rate (Hz)")
parser.add_argument("--window", type=int, default=600, help="Sliding window length (samples)")
args = parser.parse_args()

# --------- Serial ----------
try:
    ser = serial.Serial(args.port, args.baud, timeout=1)
    time.sleep(0.5)  # đợi ổn định cổng
except Exception as e:
    print(f"[ERR] Cannot open serial {args.port}: {e}")
    sys.exit(1)

# --------- Parser ----------
# Ví dụ log của bạn:
# Roll: -175.64  Pitch: 9.64  Yaw: -174.53  ax: -0.063  ay: -0.040  az: -1.020  dt(ms): 2.74
line_re = re.compile(
    r"Roll:\s*([-\d\.]+).*Pitch:\s*([-\d\.]+).*Yaw:\s*([-\d\.]+).*"
    r"ax:\s*([-\d\.]+).*ay:\s*([-\d\.]+).*az:\s*([-\d\.]+).*"
    r"dt\(ms\):\s*([-\d\.]+)"
)

def parse_line(s):
    m = line_re.search(s)
    if not m: return None
    roll = float(m.group(1))
    pitch = float(m.group(2))
    yaw = float(m.group(3))
    ax = float(m.group(4))
    ay = float(m.group(5))
    az = float(m.group(6))
    dt_ms = float(m.group(7))
    return {"roll": roll, "pitch": pitch, "yaw": yaw,
            "ax": ax, "ay": ay, "az": az, "dt_ms": dt_ms}

# --------- Unwrap góc để tránh nhảy biên ±180° ----------
def wrap180(a):
    if a > 180.0: a -= 360.0
    if a <= -180.0: a += 360.0
    return a

yaw_init = False
yaw_cont = 0.0
def unwrap_yaw(y_wrapped):
    global yaw_init, yaw_cont
    y = wrap180(y_wrapped)
    if not yaw_init:
        yaw_cont = y
        yaw_init = True
        return yaw_cont
    delta = y - wrap180(yaw_cont)
    if delta > 180.0: delta -= 360.0
    elif delta <= -180.0: delta += 360.0
    yaw_cont += delta
    return yaw_cont

# --------- Buffers ----------
N = args.window
t_buf  = deque(maxlen=N)
roll_buf  = deque(maxlen=N)
pitch_buf = deque(maxlen=N)
yaw_buf   = deque(maxlen=N)        # wrap
yawc_buf  = deque(maxlen=N)        # continuous (unwrap)
ax_buf    = deque(maxlen=N)
ay_buf    = deque(maxlen=N)
az_buf    = deque(maxlen=N)

# Low-pass nhẹ chỉ cho hiển thị
def ema(prev, x, alpha=0.15):
    return prev + alpha * (x - prev)

yaw_f = 0.0

# CSV (tuỳ chọn)
csv_writer = None
csv_file = None
if args.save_csv:
    csv_file = open(args.save_csv, "w", newline="")
    csv_writer = csv.writer(csv_file)
    csv_writer.writerow(["time_s","roll","pitch","yaw_wrap","yaw_cont","ax","ay","az","dt_ms"])

# --------- Matplotlib Figure ----------
plt.style.use("ggplot")
fig = plt.figure(figsize=(12, 6))

# Subplot 1: time series angles
ax1 = fig.add_subplot(1,2,1)
ax1.set_title("Góc (deg) theo thời gian")
ax1.set_xlabel("Thời gian (s)")
ax1.set_ylabel("Góc (deg)")
l_roll, = ax1.plot([], [], label="Roll")
l_pitch,= ax1.plot([], [], label="Pitch")
l_yaw,  = ax1.plot([], [], label="Yaw (wrap)")
l_yawc, = ax1.plot([], [], label="Yaw (cont)")
ax1.legend()
ax1.set_ylim(-190, 190)

# Subplot 2: 3D trục body (orientation)
ax2 = fig.add_subplot(1,2,2, projection="3d")
ax2.set_title("Trục body trong không gian")
ax2.set_xlim(-1, 1); ax2.set_ylim(-1,1); ax2.set_zlim(-1,1)
ax2.set_xlabel("X"); ax2.set_ylabel("Y"); ax2.set_zlabel("Z")
# Vẽ gốc/world axes
ax2.plot([0,1],[0,0],[0,0], color="gray", linewidth=1)
ax2.plot([0,0],[0,1],[0,0], color="gray", linewidth=1)
ax2.plot([0,0],[0,0],[0,1], color="gray", linewidth=1)
# Vật thể: vẽ 3 trục body Xb,Yb,Zb
bx_line, = ax2.plot([], [], [], color="r", linewidth=2, label="Xb")
by_line, = ax2.plot([], [], [], color="g", linewidth=2, label="Yb")
bz_line, = ax2.plot([], [], [], color="b", linewidth=2, label="Zb")
ax2.legend(loc="upper left")

def euler_to_R(roll_deg, pitch_deg, yaw_deg):
    # Giả định yaw(Z) - pitch(Y) - roll(X) (aerospace convention)
    r = np.deg2rad(roll_deg)
    p = np.deg2rad(pitch_deg)
    y = np.deg2rad(yaw_deg)
    Rx = np.array([[1,0,0],[0,np.cos(r),-np.sin(r)],[0,np.sin(r),np.cos(r)]])
    Ry = np.array([[np.cos(p),0,np.sin(p)],[0,1,0],[-np.sin(p),0,np.cos(p)]])
    Rz = np.array([[np.cos(y),-np.sin(y),0],[np.sin(y),np.cos(y),0],[0,0,1]])
    R = Rz @ Ry @ Rx
    return R

start_t = time.time()

# --------- Animation update ----------
def update(_frame):
    global yaw_f
    line = ser.readline().decode(errors="ignore").strip()
    data = parse_line(line)
    if data is None:
        return l_roll, l_pitch, l_yaw, l_yawc, bx_line, by_line, bz_line

    t = time.time() - start_t
    roll = data["roll"]
    pitch= data["pitch"]
    yaw_wrapped = data["yaw"]
    yaw_continuous = unwrap_yaw(yaw_wrapped)
    yaw_f = ema(yaw_f, yaw_continuous, alpha=0.15)

    ax_val = data["ax"]
    ay_val = data["ay"]
    az_val = data["az"]

    # push buffers
    t_buf.append(t)
    roll_buf.append(roll)
    pitch_buf.append(pitch)
    yaw_buf.append(yaw_wrapped)
    yawc_buf.append(yaw_continuous)
    ax_buf.append(ax_val)
    ay_buf.append(ay_val)
    az_buf.append(az_val)

    if csv_writer:
        csv_writer.writerow([t, roll, pitch, yaw_wrapped, yaw_continuous, ax_val, ay_val, az_val, data["dt_ms"]])

    # update 2D lines
    l_roll.set_data(t_buf, roll_buf)
    l_pitch.set_data(t_buf, pitch_buf)
    l_yaw.set_data(t_buf, yaw_buf)
    l_yawc.set_data(t_buf, yawc_buf)
    # autoscale x
    ax1.set_xlim(max(0, t - (args.window/args.hz)), t)

    # update 3D triad by Euler
    R = euler_to_R(roll, pitch, yaw_f)  # dùng yaw đã mượt để hiển thị ổn định
    origin = np.array([0,0,0])
    Xb = R @ np.array([1,0,0]) * 0.8
    Yb = R @ np.array([0,1,0]) * 0.8
    Zb = R @ np.array([0,0,1]) * 0.8
    bx_line.set_data_3d([origin[0], Xb[0]], [origin[1], Xb[1]], [origin[2], Xb[2]])
    by_line.set_data_3d([origin[0], Yb[0]], [origin[1], Yb[1]], [origin[2], Yb[2]])
    bz_line.set_data_3d([origin[0], Zb[0]], [origin[1], Zb[1]], [origin[2], Zb[2]])

    return l_roll, l_pitch, l_yaw, l_yawc, bx_line, by_line, bz_line

interval_ms = int(1000.0 / args.hz)
ani = FuncAnimation(fig, update, interval=interval_ms, blit=False)

# print(f"[INFO] Reading {args.port} @ {args.baud} andprint(f"[INFO] Reading {args.port} @ {args.baud} and plotting at ~{args.hz} Hz"))
try:
    plt.show()
finally:
    if csv_file:
        csv_file.close()
