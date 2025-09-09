// Kết nối tới MQTT Broker
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");

client.on("connect", () => {
  console.log("✅ Connected to MQTT broker");

  // Subscribe tất cả topic
  for (let i = 1; i <= 4; i++) {
    ["acc/x", "acc/y", "acc/z", "gyro/x", "gyro/y", "gyro/z", "tmpture"].forEach(topic => {
      client.subscribe(`bridge/node${i}/${topic}`);
    });
  }
});

// Mapping topic → dataset index
const topicMap = {
  "bridge/node1/acc/x": { chart:"accX", index:0 },
  "bridge/node2/acc/x": { chart:"accX", index:1 },
  "bridge/node3/acc/x": { chart:"accX", index:2 },
  "bridge/node4/acc/x": { chart:"accX", index:3 },

  "bridge/node1/acc/y": { chart:"accY", index:0 },
  "bridge/node2/acc/y": { chart:"accY", index:1 },
  "bridge/node3/acc/y": { chart:"accY", index:2 },
  "bridge/node4/acc/y": { chart:"accY", index:3 },

  "bridge/node1/acc/z": { chart:"accZ", index:0 },
  "bridge/node2/acc/z": { chart:"accZ", index:1 },
  "bridge/node3/acc/z": { chart:"accZ", index:2 },
  "bridge/node4/acc/z": { chart:"accZ", index:3 },

  "bridge/node1/gyro/x": { chart:"gyroX", index:0 },
  "bridge/node2/gyro/x": { chart:"gyroX", index:1 },
  "bridge/node3/gyro/x": { chart:"gyroX", index:2 },
  "bridge/node4/gyro/x": { chart:"gyroX", index:3 },

  "bridge/node1/gyro/y": { chart:"gyroY", index:0 },
  "bridge/node2/gyro/y": { chart:"gyroY", index:1 },
  "bridge/node3/gyro/y": { chart:"gyroY", index:2 },
  "bridge/node4/gyro/y": { chart:"gyroY", index:3 },

  "bridge/node1/gyro/z": { chart:"gyroZ", index:0 },
  "bridge/node2/gyro/z": { chart:"gyroZ", index:1 },
  "bridge/node3/gyro/z": { chart:"gyroZ", index:2 },
  "bridge/node4/gyro/z": { chart:"gyroZ", index:3 },

  "bridge/node1/tmpture": { chart:"temp", index:0 },
  "bridge/node2/tmpture": { chart:"temp", index:1 },
  "bridge/node3/tmpture": { chart:"temp", index:2 },
  "bridge/node4/tmpture": { chart:"temp", index:3 }
};

client.on("message", (topic, message) => {
  let val = parseFloat(message.toString());
  if (isNaN(val)) return;

  let match = topic.match(/bridge\/node(\d+)\/(acc|gyro|tmpture)\/?([xyz])?/);
  if (!match) return;

  if(isFiltering) return;

  let nodeIndex = parseInt(match[1]) - 1;
  let type = match[2]; // acc, gyro, tmpture
  let axis = match[3]; // x,y,z or undefined

  let chartKey = "";
  if (type === "acc") chartKey = "acc" + axis.toUpperCase();
  else if (type === "gyro") chartKey = "gyro" + axis.toUpperCase();
  else if (type === "tmpture") chartKey = "temp";

  if (chartKey && window.updateChart) {
    window.updateChart(chartKey, nodeIndex, val);
  }
});