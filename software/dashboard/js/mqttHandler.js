// Kết nối tới MQTT Broker qua WebSocket
const client = mqtt.connect("wss://broker.hivemq.com:8884/mqtt");
// const client = mqtt.connect("ws://broker.hivemq.com:8000/mqtt", {
//   clientId: "webClient_" + Math.random().toString(16).substr(2, 8)
// });

// Khi kết nối thành công
client.on("connect", () => {
  console.log("✅ Connected to MQTT broker");
  // Subcribe các topic của 4 node
  client.subscribe("bridge/node1/acc/x");
  client.subscribe("bridge/node1/acc/y");
  client.subscribe("bridge/node1/acc/z");
  client.subscribe("bridge/node1/gyro/x");
  client.subscribe("bridge/node1/gyro/y");
  client.subscribe("bridge/node1/gyro/z");
  client.subscribe("bridge/node1/tmpture");

  client.subscribe("bridge/node2/acc/x");
  client.subscribe("bridge/node2/acc/y");
  client.subscribe("bridge/node2/acc/z");
  client.subscribe("bridge/node2/gyro/x");
  client.subscribe("bridge/node2/gyro/y");
  client.subscribe("bridge/node2/gyro/z");
  client.subscribe("bridge/node2/tmpture");

  client.subscribe("bridge/node3/acc/x");
  client.subscribe("bridge/node3/acc/y");
  client.subscribe("bridge/node3/acc/z");
  client.subscribe("bridge/node3/gyro/x");
  client.subscribe("bridge/node3/gyro/y");
  client.subscribe("bridge/node3/gyro/z");
  client.subscribe("bridge/node3/tmpture");

  client.subscribe("bridge/node4/acc/x");
  client.subscribe("bridge/node4/acc/y");
  client.subscribe("bridge/node4/acc/z");
  client.subscribe("bridge/node4/gyro/x");
  client.subscribe("bridge/node4/gyro/y");
  client.subscribe("bridge/node4/gyro/z");
  client.subscribe("bridge/node4/tmpture");
});

let lastLabel = null;

// Lắng nghe dữ liệu từ MQTT
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
    let timeLabel = time.toLocaleTimeString();

    // luôn lưu dữ liệu đầy đủ
    fullData.labels.push(time);
    fullData.datasets[nodeIndex].push({ t: time, v: payload });

    // nếu đang filter thì không update chart
    if (isFiltering) return;

    // Nếu label mới khác label cuối cùng → thêm label mới
    if (timeLabel !== lastLabel) {
      chart.data.labels.push(timeLabel);
      lastLabel = timeLabel;
    }

    // thêm giá trị vào đúng dataset
    chart.data.datasets[nodeIndex].data.push(payload);

    // nếu vượt quá MAX_POINTS thì dịch chart
    if (chart.data.labels.length > MAX_POINTS) {
        chart.data.labels.shift();
        for (let i = 0; i < 4; i++) {
          chart.data.datasets[i].data.shift();
        }
      }

      // Force update ngay lập tức, không cần zoom/pan
      chart.update('none');
    }
});