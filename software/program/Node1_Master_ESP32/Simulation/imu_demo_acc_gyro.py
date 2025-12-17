#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Demo mô phỏng IMU chỉ với ACC + GYRO
- Sinh dữ liệu synthetic (acc: g, gyro: deg/s) và ghi ra demo_acc_gyro.csv
- Tính orientation (Mahony IMU) và xấp xỉ vị trí (dead-reckoning) để vẽ tam trục body di chuyển
Cách chạy:
    python imu_demo_acc_gyro.py
Yêu cầu:
    pip install matplotlib numpy
"""
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import csv, time
from math import sin, cos, pi

class MahonyIMU:
    def __init__(self, sample_freq=200.0, twoKp=1.0, twoKi=0.0):
        self.sampleFreq = sample_freq
        self.twoKp = twoKp
        self.twoKi = twoKi
        self.q = np.array([1.0, 0.0, 0.0, 0.0], dtype=float)
        self.integralFB = np.zeros(3, dtype=float)
    def set_dt(self, dt):
        if dt > 0: self.sampleFreq = 1.0/dt
    @staticmethod
    def _inv_sqrt(x):
        return 1.0/np.sqrt(x + 1e-12)
    def updateIMU(self, gx_dps, gy_dps, gz_dps, ax_g, ay_g, az_g):
        gx = np.radians(gx_dps); gy = np.radians(gy_dps); gz = np.radians(gz_dps)
        q0,q1,q2,q3 = self.q
        norm_acc = np.sqrt(ax_g*ax_g + ay_g*ay_g + az_g*az_g)
        if norm_acc >= 1e-6:
            ax, ay, az = ax_g/norm_acc, ay_g/norm_acc, az_g/norm_acc
            halfvx = q1*q3 - q0*q2
            halfvy = q0*q1 + q2*q3
            halfvz = q0*q0 - 0.5 + q3*q3
            halfex = (ay*halfvz - az*halfvy)
            halfey = (az*halfvx - ax*halfvz)
            halfez = (ax*halfvy - ay*halfvx)
            if self.twoKi > 0.0:
                dt = 1.0 / self.sampleFreq
                self.integralFB += self.twoKi * np.array([halfex, halfey, halfez]) * dt
                gx += self.integralFB[0]; gy += self.integralFB[1]; gz += self.integralFB[2]
            else:
                self.integralFB[:] = 0.0
            gx += self.twoKp * halfex
            gy += self.twoKp * halfey
            gz += self.twoKp * halfez
        dt_half = 0.5 * (1.0/self.sampleFreq)
        qa,qb,qc = q0,q1,q2
        q0 += (-qb*gx - qc*gy - q3*gz) * dt_half
        q1 += ( qa*gx + qc*gz - q3*gy) * dt_half
        q2 += ( qa*gy - qb*gz + q3*gx) * dt_half
        q3 += ( qa*gz + qb*gy - qc*gx) * dt_half
        recip = self._inv_sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3)
        self.q = np.array([q0,q1,q2,q3]) * recip
    def euler_deg(self):
        q0,q1,q2,q3 = self.q
        roll  = np.degrees(np.arctan2(2.0*(q0*q1 + q2*q3), 1.0 - 2.0*(q1*q1 + q2*q2)))
        pitch = np.degrees(np.arcsin(np.clip(2.0*(q0*q2 - q3*q1), -1.0, 1.0)))
        yaw   = np.degrees(np.arctan2(2.0*(q0*q3 + q1*q2), 1.0 - 2.0*(q2*q2 + q3*q3)))
        return float(roll), float(pitch), float(yaw)

class PositionDR:
    def __init__(self, g=9.80665, vel_damp=0.995, zupt_acc_thresh=0.20, zupt_ang_thresh=6.0):
        self.G = g
        self.vel = np.zeros(3, dtype=float)
        self.pos = np.zeros(3, dtype=float)
        self.vel_damp = vel_damp
        self.zupt_acc_thresh = zupt_acc_thresh
        self.zupt_ang_thresh = zupt_ang_thresh
        self.prev_euler = None
    @staticmethod
    def euler_to_R(roll_deg, pitch_deg, yaw_deg):
        r = np.radians(roll_deg); p = np.radians(pitch_deg); y = np.radians(yaw_deg)
        Rx = np.array([[1,0,0],[0,np.cos(r),-np.sin(r)],[0,np.sin(r),np.cos(r)]])
        Ry = np.array([[np.cos(p),0,np.sin(p)],[0,1,0],[-np.sin(p),0,np.cos(p)]])
        Rz = np.array([[np.cos(y),-np.sin(y),0],[np.sin(y),np.cos(y),0],[0,0,1]])
        return Rz @ Ry @ Rx
    def step(self, roll, pitch, yaw, ax_g, ay_g, az_g, dt_s):
        R = self.euler_to_R(roll, pitch, yaw)
        a_body_mps2 = np.array([ax_g, ay_g, az_g]) * self.G
        a_world = R @ a_body_mps2
        a_lin = a_world - np.array([0.0,0.0,-self.G])
        zupt = False
        if self.prev_euler is not None:
            pr,pp,py = self.prev_euler
            d_ang = abs(roll-pr) + abs(pitch-pp) + abs(yaw-py)
            zupt = (np.linalg.norm(a_lin) < self.zupt_acc_thresh) and (d_ang < self.zupt_ang_thresh)
        if zupt:
            self.vel[:] = 0.0
        else:
            self.vel += a_lin * dt_s
            self.vel *= self.vel_damp
            self.pos += self.vel * dt_s
        self.prev_euler = (roll, pitch, yaw)
        return self.pos.copy(), self.vel.copy()

# ===== Quaternion helpers for synthetic generation =====
def quat_prod(q, r):
    w1,x1,y1,z1 = q; w2,x2,y2,z2 = r
    return np.array([
        w1*w2 - x1*x2 - y1*y2 - z1*z2,
        w1*x2 + x1*w2 + y1*z2 - z1*y2,
        w1*y2 - x1*z2 + y1*w2 + z1*x2,
        w1*z2 + x1*y2 - y1*x2 + z1*w2
    ], dtype=float)

def quat_to_R(q):
    w,x,y,z = q
    return np.array([
        [1-2*(y*y+z*z),   2*(x*y - z*w),   2*(x*z + y*w)],
        [2*(x*y + z*w),   1-2*(x*x+z*z),   2*(y*z - x*w)],
        [2*(x*z - y*w),   2*(y*z + x*w),   1-2*(x*x+y*y)]
    ], dtype=float)

# ===== Synthetic data generation =====
def generate_synthetic_acc_gyro(duration_s=15.0, fs=200.0, radius_m=0.12):
    dt = 1.0/fs; N = int(duration_s*fs); t = np.arange(N)*dt
    wx = np.radians(30.0) * np.sin(2*pi*0.20 * t)
    wy = np.radians(20.0) * np.sin(2*pi*0.10 * t)
    wz = np.radians(60.0) * np.sin(2*pi*0.15 * t)
    q = np.array([1.0,0.0,0.0,0.0], dtype=float); q_list=[]
    for k in range(N):
        w = np.array([0.0, wx[k], wy[k], wz[k]])
        qdot = 0.5 * quat_prod(q, w)
        q = q + qdot*dt
        q = q / (np.linalg.norm(q) + 1e-12)
        q_list.append(q.copy())
    Rm = radius_m; freq_xy=0.10; freq_z=0.05
    G = 9.80665
    ax_g = np.zeros(N); ay_g=np.zeros(N); az_g=np.zeros(N)
    gx_dps=np.degrees(wx); gy_dps=np.degrees(wy); gz_dps=np.degrees(wz)
    for k in range(N):
        tt=t[k]
        # gia tốc world từ quỹ đạo analytic
        acc_world = np.array([
            -Rm * (2*pi*freq_xy)**2 * np.cos(2*pi*freq_xy*tt),
            -Rm * (2*pi*freq_xy)**2 * np.sin(2*pi*freq_xy*tt),
            -0.05* (2*pi*freq_z )**2 * np.sin(2*pi*freq_z*tt)
        ])
        a_world = acc_world + np.array([0.0,0.0,-G])
        R = quat_to_R(q_list[k])  # body->world
        a_body = R.T @ a_world
        ax_g[k], ay_g[k], az_g[k] = a_body / G
    return dt, t, gx_dps, gy_dps, gz_dps, ax_g, ay_g, az_g

# ===== Main =====
def main():
    Dt, T, Gx, Gy, Gz, Ax, Ay, Az = generate_synthetic_acc_gyro()
    N = len(T)
    csv_name='demo_acc_gyro.csv'
    with open(csv_name,'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['dt_ms','gx','gy','gz','ax','ay','az'])
        for k in range(N):
            w.writerow([Dt*1000.0, Gx[k], Gy[k], Gz[k], Ax[k], Ay[k], Az[k]])
    print(f"[INFO] Đã sinh {N} mẫu vào '{csv_name}' (fs={1.0/Dt:.1f} Hz)")

    filt = MahonyIMU(sample_freq=1.0/Dt, twoKp=1.0, twoKi=0.0)
    dr   = PositionDR(g=9.80665, vel_damp=0.995, zupt_acc_thresh=0.20, zupt_ang_thresh=6.0)

    plt.style.use('ggplot')
    fig = plt.figure(figsize=(12,6))
    ax1 = fig.add_subplot(1,2,1)
    ax1.set_title('Góc (độ) theo thời gian')
    ax1.set_xlabel('Thời gian (s)')
    ax1.set_ylabel('Góc (độ)')
    l_roll, = ax1.plot([], [], label='Roll')
    l_pitch,= ax1.plot([], [], label='Pitch')
    l_yaw,  = ax1.plot([], [], label='Yaw')
    ax1.legend(); ax1.set_ylim(-190,190)

    ax2 = fig.add_subplot(1,2,2, projection='3d')
    ax2.set_title('Tam trục body di chuyển trong không gian')
    ax2.set_xlabel('X (m)'); ax2.set_ylabel('Y (m)'); ax2.set_zlabel('Z (m)')
    ax2.set_xlim(-0.3,0.3); ax2.set_ylim(-0.3,0.3); ax2.set_zlim(-0.3,0.3)
    ax2.plot([0,0.2],[0,0],[0,0], color='gray', linewidth=1)
    ax2.plot([0,0],[0,0.2],[0,0], color='gray', linewidth=1)
    ax2.plot([0,0],[0,0],[0,0.2], color='gray', linewidth=1)
    bx_line, = ax2.plot([], [], [], color='r', linewidth=2, label='Xb')
    by_line, = ax2.plot([], [], [], color='g', linewidth=2, label='Yb')
    bz_line, = ax2.plot([], [], [], color='b', linewidth=2, label='Zb')
    ax2.legend(loc='upper left')

    t_buf=[]; roll_buf=[]; pitch_buf=[]; yaw_buf=[]
    yaw_f = 0.0; alpha_yaw = 0.15

    idx = 0
    def update(_):
        nonlocal idx, yaw_f
        if idx >= N:
            return l_roll, l_pitch, l_yaw, bx_line, by_line, bz_line
        dt_s = Dt
        gx,gy,gz = Gx[idx], Gy[idx], Gz[idx]
        ax,ay,az = Ax[idx], Ay[idx], Az[idx]
        filt.set_dt(dt_s)
        filt.updateIMU(gx, gy, gz, ax, ay, az)
        roll, pitch, yaw = filt.euler_deg()
        pos, vel = dr.step(roll, pitch, yaw, ax, ay, az, dt_s)
        yaw_f = yaw_f + alpha_yaw * (yaw - yaw_f)
        R = dr.euler_to_R(roll, pitch, yaw_f)
        origin = pos
        Xb = R @ np.array([0.12,0,0]) + origin
        Yb = R @ np.array([0,0.12,0]) + origin
        Zb = R @ np.array([0,0,0.12]) + origin
        bx_line.set_data_3d([origin[0], Xb[0]],[origin[1], Xb[1]],[origin[2], Xb[2]])
        by_line.set_data_3d([origin[0], Yb[0]],[origin[1], Yb[1]],[origin[2], Yb[2]])
        bz_line.set_data_3d([origin[0], Zb[0]],[origin[1], Zb[1]],[origin[2], Zb[2]])
        t_buf.append(T[idx]); roll_buf.append(roll); pitch_buf.append(pitch); yaw_buf.append(yaw)
        l_roll.set_data(t_buf, roll_buf); l_pitch.set_data(t_buf, pitch_buf); l_yaw.set_data(t_buf, yaw_buf)
        t_last = t_buf[-1]; ax1.set_xlim(max(0.0, t_last-10.0), t_last)
        ax2.set_xlim(origin[0]-0.3, origin[0]+0.3)
        ax2.set_ylim(origin[1]-0.3, origin[1]+0.3)
        ax2.set_zlim(origin[2]-0.3, origin[2]+0.3)
        idx += 1
        return l_roll, l_pitch, l_yaw, bx_line, by_line, bz_line

    ani = FuncAnimation(fig, update, interval=int(1000*Dt), blit=False)
    print('[INFO] Đang mô phỏng...')
    plt.tight_layout(); plt.show()

if __name__ == '__main__':
    main()
