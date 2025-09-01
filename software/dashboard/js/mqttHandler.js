// Kết nối tới MQTT Broker qua WebSocket
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");

client.on("connect", () => {
  console.log("✅ Connected to MQTT broker");
  // Subcribe các topic của 4 node
  client.subscribe("bridge/node1/acc/x");
  client.subscribe("bridge/node2/acc/x");
  client.subscribe("bridge/node3/acc/x");
  client.subscribe("bridge/node4/acc/x");
});

client.on("message", function (topic, message) {
  const payload = parseFloat(message.toString());
  if (isNaN(payload)) return;

  let nodeIndex;
  if (topic.includes("node1/acc/x")) nodeIndex = 0;
  if (topic.includes("node2/acc/x")) nodeIndex = 1;
  if (topic.includes("node3/acc/x")) nodeIndex = 2;
  if (topic.includes("node4/acc/x")) nodeIndex = 3;

  if (nodeIndex !== undefined) {
    let time = new Date();

    // luôn lưu dữ liệu đầy đủ
    fullData.datasets[nodeIndex].push({ x: time, y: payload });

    // nếu đang filter thì không update chart realtime
    if (isFiltering) return;

    // realtime update
    for (let i = 0; i < 4; i++) {
      let data = fullData.datasets[i];
      let startIndex = Math.max(0, data.length - MAX_POINTS);
      chart.data.datasets[i].data = data.slice(startIndex);
    }

    chart.update('none');
  }
});
