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
    else
    {
      // tìm node có nhiều điểm nhất
      let longestNode = fullData.datasets.reduce((a, b) =>
      a.length > b.length ? a : b
      );

      // cập nhật trục X = timestamps của node dài nhất
      chart.data.labels = longestNode.map(p => p.t.toLocaleTimeString());

      // cập nhật dữ liệu từng node
      chart.data.datasets.forEach((ds, i) => {
      ds.data = fullData.datasets[i].map(obj => obj.v);
      });
      
      chart.update('none');
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

    // ⚡ force update ngay lập tức, không cần zoom/pan
    chart.update('none');
    }
});