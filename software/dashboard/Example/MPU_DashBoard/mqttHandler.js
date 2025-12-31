
/* mqttHandler.js — JSON one message per sample (final) */
(function () {
  // Dùng wss khi host web là https. Nếu bạn đang chạy http/file:// thì ws://:8000 vẫn được.
  const MQTT_WSS_URL = (location.protocol === 'https:' )
    ? "wss://broker.hivemq.com:8884/mqtt"
    : "ws://broker.hivemq.com:8000/mqtt";

  const MAX_NODES = 128;

  // Yêu cầu đã nhúng mqtt.js (browser) trước file này:
  // https://unpkg.com/mqtt/dist/mqtt.min.js</script>

  const client = mqtt.connect(MQTT_WSS_URL, {
    keepalive: 60,
    reconnectPeriod: 2000, // 2s auto-reconnect
    connectTimeout: 10_000
  });

  client.on("connect", () => {
    console.log("✅ Connected to MQTT broker (WSS)", MQTT_WSS_URL);
    // Subscribe IMU JSON: bridge/<area>/<node>/imu
    client.subscribe("bridge/+/+/imu", { qos: 1 }, (err) => {
      if (err) console.error("❌ Subscribe error:", err);
    });
  });

  client.on("reconnect", () => console.log("↻ Reconnecting MQTT..."));
  client.on("close", () => console.log("✖ MQTT connection closed"));
  client.on("error", (err) => console.error("❌ MQTT error:", err?.message || err));

  function nodeIdToIndex(nodeIdStr) {
    // "N01" -> 0, "N12" -> 11; nếu chỉ là "1" -> 0
    const m = String(nodeIdStr).match(/^N?(\d+)$/i);
    if (!m) return 0;
    const idNum = parseInt(m[1], 10);
    return Math.max(0, Math.min(MAX_NODES - 1, idNum - 1));
  }

  // Gọi updateChart nếu có sẵn ở app.js
  function safeUpdateChart(chartKey, nodeIndex, value) {
    if (typeof window.updateChart === "function") {
      window.updateChart(chartKey, nodeIndex, value);
    } else {
      // Nếu chưa load xong app.js, có thể buffer hoặc log.
      // console.warn("updateChart not ready yet");
    }
  }

  client.on("message", (topic, buf) => {
    // Debug payload (bật khi cần)
    // console.log("[MQTT] msg on", topic, "payload:", buf.toString());

    const m = topic.match(/^bridge\/([^/]+)\/([^/]+)\/imu$/);
    if (!m) return;

    const nodeStr = m[2]; // ví dụ "N01"
    const nodeIndex = nodeIdToIndex(nodeStr); // "N01"->0, "N02"->1 ...

    let msg;
    try {
      msg = JSON.parse(buf.toString().trim());
    } catch (e) {
      console.warn("⚠️ JSON parse error:", e);
      return;
    }

    // Chỉ cập nhật acc/gyro theo yêu cầu
    if (typeof msg.ax === "number") safeUpdateChart("accX",  nodeIndex, msg.ax);
    if (typeof msg.ay === "number") safeUpdateChart("accY",  nodeIndex, msg.ay);
    if (typeof msg.az === "number") safeUpdateChart("accZ",  nodeIndex, msg.az);
    if (typeof msg.gx === "number") safeUpdateChart("gyroX", nodeIndex, msg.gx);
    if (typeof msg.gy === "number") safeUpdateChart("gyroY", nodeIndex, msg.gy);
    if (typeof msg.gz === "number") safeUpdateChart("gyroZ", nodeIndex, msg.gz);

    // Không cập nhật temp/battery theo yêu cầu hiện tại.
  });

  // expose client để debug
  window.__mqttClient = client;
})();
