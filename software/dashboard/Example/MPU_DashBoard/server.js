
const path = require('path');
const express = require('express');
const cors = require('cors');
const mongoose = require('mongoose');
const mqtt = require('mqtt'); // ✅ add this

const app = express();

// ===== Middleware =====
app.use(cors());
app.use(express.json());
app.use(express.static(__dirname));

// ===== Config =====
const PORT = process.env.PORT || 3019;
const MONGO_URL = process.env.MONGO_URL || 'mongodb://localhost:27017';
const MONGO_DB = process.env.MONGO_DB || 'bridge_shm';

// MQTT configs (server-side subscriber)
const MQTT_URL  = process.env.MQTT_URL  || 'mqtt://broker.hivemq.com:1883'; // TCP MQTT
const MQTT_QOS  = Number(process.env.MQTT_QOS || 1);
const MQTT_TOPIC = process.env.MQTT_TOPIC || 'bridge/+/+/imu';

// ===== Kết nối MongoDB & start server =====
(async () => {
  try {
    await mongoose.connect(MONGO_URL, {
      dbName: MONGO_DB,
      serverSelectionTimeoutMS: 10000,
    });
    console.log(`✅ MongoDB connected: ${MONGO_URL}/${MONGO_DB}`);

    // --- Start HTTP server
    app.listen(PORT, () => console.log(`🚀 Server started at http://localhost:${PORT}`));

    // --- After Mongo is ready, start MQTT -> Mongo bridge ---
    startMqttMongoBridge();

  } catch (err) {
    console.error('❌ Mongo connect error:', err);
    process.exit(1);
  }
})();

mongoose.connection.on('error', (err) => console.error('Mongoose connection error:', err));
mongoose.connection.on('disconnected', () => console.log('Mongoose disconnected'));

// ===== Schema & Model =====
const sampleSchema = new mongoose.Schema(
  {
    time: { type: Date, required: true },        // server receive time
    area: { type: String, default: 1 },
    name_node: { type: String, required: true }, // "N01"
    ax: { type: Number, default: null },
    ay: { type: Number, default: null },
    az: { type: Number, default: null },
    gx: { type: Number, default: null },
    gy: { type: Number, default: null },
    gz: { type: Number, default: null },
    temp: { type: Number, default: null },
  },
  { 
    collection: 'imu_samples',
    versionKey: false,
  }
);
sampleSchema.index({ name_node: 1, time: -1 }); // existing index
const Sample = mongoose.model('Sample', sampleSchema);

// ===== Routes =====
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'index.html'));
});

// ===== MQTT -> Mongo bridge (all inside server.js) =====
function startMqttMongoBridge() {
  const client = mqtt.connect(MQTT_URL, {
    keepalive: 60,
    reconnectPeriod: 2000, // 2s auto-reconnect
    connectTimeout: 10_000,
  });

  client.on('connect', () => {
    console.log(`✅ MQTT connected: ${MQTT_URL}`);
    client.subscribe(MQTT_TOPIC, { qos: MQTT_QOS }, (err) => {
      if (err) console.error('❌ MQTT subscribe error:', err);
      else console.log(`📡 Subscribed: ${MQTT_TOPIC} (QoS ${MQTT_QOS})`);
    });
  });

  client.on('reconnect', () => console.log('🔄 MQTT reconnecting...'));
  client.on('close', () => console.log('✖ MQTT connection closed'));
  client.on('error', (err) => console.error('❌ MQTT error:', err?.message || err));

  client.on('message', async (topic, buf) => {
    // Expect "bridge/<area>/<node>/imu"
    const m = topic.match(/^bridge\/([^/]+)\/([^/]+)\/imu$/);
    if (!m) return;
    const area = String(m[1]);
    const name_node = String(m[2]);

    let msg;
    try {
      msg = JSON.parse(buf.toString().trim());
    } catch (e) {
      console.warn('⚠️ JSON parse error:', e);
      return;
    }

    // Normalize values (number or null)
    const toNum = (v) => (typeof v === 'number' ? v : (v == null ? null : Number(v)));
    const doc = {
      time: new Date(), // server receive time
      area,
      name_node,
      ax: toNum(msg.ax),
      ay: toNum(msg.ay),
      az: toNum(msg.az),
      gx: toNum(msg.gx),
      gy: toNum(msg.gy),
      gz: toNum(msg.gz),
      temp: toNum(msg.temp),
    };

    try {
      await Sample.create(doc);
      // console.log('➕ Saved sample:', doc);
    } catch (e) {
      console.error('❌ Mongo insert error:', e);
    }
  });

  return client;
}
