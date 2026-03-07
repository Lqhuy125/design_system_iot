
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
    // client.subscribe("bridge/+/+/imu", { qos: 1 }, (err) => {
    client.subscribe("bridge/+/cipher", { qos: 1 }, (err) => {
      if (err) console.error("❌ Subscribe error:", err);
    });
  });

  client.on("reconnect", () => console.log("↻ Reconnecting MQTT..."));
  client.on("close", () => console.log("✖ MQTT connection closed"));
  client.on("error", (err) => console.error("❌ MQTT error:", err?.message || err));

  function formatNodeId(id){
    const n = (typeof id === 'number') ? id : parseInt(String(id).replace(/\D/g,''),10);
    if (!Number.isFinite(n)) return String(id);
    return 'N' + String(n).padStart(2,'0');
  }
  function nodeIdToIndex(nodeIdStr) {
    // "N01" -> 0, "N12" -> 11; nếu chỉ là "1" -> 0
    /* const m = String(nodeIdStr).match(/^N?(\d+)$/i);
    if (!m) return 0;
    const idNum = parseInt(m[1], 10);
    return Math.max(0, Math.min(MAX_NODES - 1, idNum - 1)); */
    return Math.max(0, Math.min(MAX_NODES - 1, (parseInt(nodeIdStr) || 1) - 1));
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

  // client.on("message", (topic, payload) => {
    // Debug payload (bật khi cần)
    // console.log("[MQTT] msg on", topic, "payload:", buf.toString());

    /* const m = topic.match(/^bridge\/([^/]+)\/([^/]+)\/imu$/);
    if (!m) return;

    const nodeStr = m[2]; // ví dụ "N01"
    const nodeIndex = nodeIdToIndex(nodeStr); // "N01"->0, "N02"->1 ...

    let msg;
    try {
      msg = JSON.parse(buf.toString().trim());
    } catch (e) {
      console.warn("⚠️ JSON parse error:", e);
      return;
    } */
  client.on('message', async (topic, payload) => {
    try{
      const m = topic.match(/^bridge\/([^/]+)\/cipher$/);
      if (!m) return; // not our topic
      const areaId = m[1];

      const text = payload.toString('utf8').trim();
      if (!text) return;
      let msg;
      try { msg = JSON.parse(text); } catch(e){ console.warn('⚠️ JSON parse error:', e); return; }

      if (!msg.cipher || typeof msg.cipher !== 'string'){
        console.warn("⚠️ Missing 'cipher' field in payload", msg);
        return;
      }

      console.groupCollapsed(`📥 MQTT bridge/${areaId}/cipher`);
      console.log('raw:', text);

      const result = await window.CryptoUtils.decryptCipherData(msg.cipher);
      if (!result.success){
        console.warn('❗ Decrypt/MIC failed:', result.error);
        if (result.imu) console.log('IMU (untrusted):', result.imu);
        console.groupEnd();
        return;
      }

      const imu = result.imu;
      const nodeIndex = nodeIdToIndex(imu.id);
      const nodeLabel = formatNodeId(imu.id);

      console.log('✅ Decrypted', {areaId, ts: msg.ts, len: msg.len, micValid: result.micValid});
      console.log(`IMU ${nodeLabel}: ax=${imu.ax?.toFixed?.(3)} ay=${imu.ay?.toFixed?.(3)} az=${imu.az?.toFixed?.(3)}`);
      console.log(`           gx=${imu.gx?.toFixed?.(2)} gy=${imu.gy?.toFixed?.(2)} gz=${imu.gz?.toFixed?.(2)} dt=${imu.dt?.toFixed?.(3)} t_s=${imu.t_s}`);
      console.groupEnd();

      // Update charts if your UI provides updateChart(chartKey, nodeIndex, value)
      if (Number.isFinite(imu.ax)) safeUpdateChart('accX', nodeIndex, imu.ax);
      if (Number.isFinite(imu.ay)) safeUpdateChart('accY', nodeIndex, imu.ay);
      if (Number.isFinite(imu.az)) safeUpdateChart('accZ', nodeIndex, imu.az);
      if (Number.isFinite(imu.gx)) safeUpdateChart('gyroX', nodeIndex, imu.gx);
      if (Number.isFinite(imu.gy)) safeUpdateChart('gyroY', nodeIndex, imu.gy);
      if (Number.isFinite(imu.gz)) safeUpdateChart('gyroZ', nodeIndex, imu.gz);

      if (typeof window.appendSummaryRow === 'function'){
        window.appendSummaryRow({
          nodeId: nodeLabel, areaId, ts: msg.ts,
          ax: imu.ax, ay: imu.ay, az: imu.az,
          gx: imu.gx, gy: imu.gy, gz: imu.gz,
          dt: imu.dt, t_s: imu.t_s,
        });
      }
    }catch(err){
      console.error('Unhandled MQTT message error:', err);
    }
  });

  // Expose for debugging
  window.__mqttClient = client;
})();
