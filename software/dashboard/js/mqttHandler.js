// Kết nối tới MQTT Broker qua WebSocket
const client = mqtt.connect("ws://broker.hivemq.com:8000/mqtt");

// Khi kết nối thành công
client.on("connect", () => {
  console.log("✅ Connected to MQTT broker");
  // Subcribe các topic của 4 node
  client.subscribe("bridge/node1");
  client.subscribe("bridge/node2");
  client.subscribe("bridge/node3");
  client.subscribe("bridge/node4");
});

client.on("message", (topic, message) => {
    let value = parseFloat(message.toString());
    let time = new Date();
    let timeLabel = time.toLocaleTimeString();
  
    let nodeIndex;
    if (topic.endsWith("node1")) nodeIndex = 0;
    if (topic.endsWith("node2")) nodeIndex = 1;
    if (topic.endsWith("node3")) nodeIndex = 2;
    if (topic.endsWith("node4")) nodeIndex = 3;
  
    if (nodeIndex !== undefined) {
      // lưu vào fullData
      fullData.labels.push(time);
      fullData.datasets[nodeIndex].push({ t: time, v: value });
  
      // update chart realtime nếu không filter
      if (!isFiltering) {
        // chỉ push label 1 lần (node1 đại diện)
        if (chart.data.labels.length < fullData.labels.length) {
            chart.data.labels.push(timeLabel);
        }  
        chart.data.datasets[nodeIndex].data.push(value);
  
        // giữ tối đa MAX_POINTS
        if (chart.data.labels.length > MAX_POINTS) {
          chart.data.labels.shift();
          for (let i = 0; i < 4; i++) {
            chart.data.datasets[i].data.shift();
          }
        }
  
        chart.update();
      }
    }
  });