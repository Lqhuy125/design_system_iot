
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Publish IMU JSON (Ax/Ay/Az/Gx/Gy/Gz) mỗi 1 giây cho 3 nodes (N01..N03)
Topic: bridge/<areaId>/<nodeId>/imu
Payload:
{
  "ts": <epoch_ms>, "areaId": <int>, "nodeId": "N0x",
  "ax": <float>, "ay": <float>, "az": <float>,
  "gx": <float>, "gy": <float>, "gz": <float>
  // "temp": <float>, "battery": <int>  # nếu muốn có thể bật thêm
}
"""

import argparse
import json
import math
import random
import signal
import sys
import time
from typing import Dict

import paho.mqtt.client as mqtt


def epoch_ms() -> int:
    return int(time.time() * 1000)


def gen_sample(t: float) -> Dict[str, float]:
    """
    Sinh dữ liệu mô phỏng tương đối "thật":
    - Ax, Ay dao động nhỏ quanh 0 với nhiễu
    - Az ~ 0.98..1.02 (giả lập thành phần trọng lực)
    - Gx, Gy, Gz dao động ±10 °/s
    """
    # dao động sine nhẹ theo thời gian t (giây) + nhiễu
    ax = 0.05 * math.sin(2 * math.pi * 0.2 * t) + random.uniform(-0.02, 0.02)
    ay = 0.05 * math.cos(2 * math.pi * 0.17 * t) + random.uniform(-0.02, 0.02)
    az = 0.98 + 0.01 * math.sin(2 * math.pi * 0.11 * t) + random.uniform(-0.005, 0.005)

    gx = 5.0 * math.sin(2 * math.pi * 0.33 * t) + random.uniform(-2.0, 2.0)
    gy = 5.0 * math.cos(2 * math.pi * 0.27 * t) + random.uniform(-2.0, 2.0)
    gz = 5.0 * math.sin(2 * math.pi * 0.41 * t + 1.2) + random.uniform(-2.0, 2.0)

    # làm tròn hợp lý (không bắt buộc)
    return {
        "ax": round(ax, 3),
        "ay": round(ay, 3),
        "az": round(az, 3),
        "gx": round(gx, 1),
        "gy": round(gy, 1),
        "gz": round(gz, 1),
    }


def make_client(client_id: str, host: str, port: int, keepalive: int = 60) -> mqtt.Client:
    client = mqtt.Client(client_id=client_id, clean_session=True, transport="tcp")
    # Nếu broker cần user/pass:
    # client.username_pw_set("user", "password")

    # Callback hữu ích khi debug
    def on_connect(c, userdata, flags, rc):
        if rc == 0:
            print(f"✅ Connected to MQTT: {host}:{port}")
        else:
            print(f"❌ Connect failed, rc={rc}")

    def on_disconnect(c, userdata, rc):
        print(f"↯ Disconnected (rc={rc})")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect

    client.connect(host, port, keepalive=keepalive)
    client.loop_start()  # chạy loop nền (reconnect, keepalive, callback)
    return client


def main():
    parser = argparse.ArgumentParser(description="IMU JSON publisher for nodes N01..N03 (1 Hz)")
    parser.add_argument("--host", default="broker.hivemq.com", help="MQTT broker host (default: broker.hivemq.com)")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port (default: 1883)")
    parser.add_argument("--area", type=int, default=1, help="areaId for topic bridge/<area>/<node>/imu (default: 1)")
    parser.add_argument("--interval", type=float, default=1.0, help="publish interval seconds (default: 1.0)")
    parser.add_argument("--qos", type=int, default=1, choices=[0, 1, 2], help="MQTT QoS (default: 1)")
    parser.add_argument("--verbose", action="store_true", help="print each published message")
    parser.add_argument("--with-temp", action="store_true", help="include temp field in payload")
    parser.add_argument("--with-battery", action="store_true", help="include battery field in payload")

    args = parser.parse_args()

    client = make_client(client_id="imu_publisher_py", host=args.host, port=args.port)

    running = True

    def handle_sigint(sig, frame):
        nonlocal running
        running = False
        print("\n⏹ Stopping publisher...")

    signal.signal(signal.SIGINT, handle_sigint)
    signal.signal(signal.SIGTERM, handle_sigint)

    # Nodes N01..N03
    node_ids = ["N01", "N02", "N03"]
    t0 = time.time()
    next_tick = t0

    try:
        while running:
            now = time.time()
            if now < next_tick:
                time.sleep(min(0.01, next_tick - now))
                continue

            ts_ms = epoch_ms()
            t = now - t0

            for node_id in node_ids:
                topic = f"bridge/{args.area}/{node_id}/imu"
                payload = {
                    "ts": ts_ms,
                    "areaId": args.area,
                    "nodeId": node_id,
                    **gen_sample(t),
                }
                if args.with_temp:
                    # ví dụ temp khoảng 27..30°C
                    payload["temp"] = round(27.0 + 3.0 * random.random(), 1)
                if args.with_battery:
                    # ví dụ pin giảm chậm, demo random quanh 90..100
                    payload["battery"] = random.randint(90, 100)

                data = json.dumps(payload, separators=(",", ":"))
                # publish QoS=1, retain=False
                res = client.publish(topic, data, qos=args.qos, retain=False)
                # chú ý: publish trả về MQTTMessageInfo; có thể gọi res.wait_for_publish() nếu muốn block

                if args.verbose:
                    print(f"→ {topic} {data}")

            next_tick += args.interval

        # kết thúc
    finally:
        client.loop_stop()
        client.disconnect()
        print("✅ Publisher stopped cleanly.")


if __name__ == "__main__":
    main()
