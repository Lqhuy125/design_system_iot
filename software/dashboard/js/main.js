// Cấu hình chung
const MAX_POINTS = 100;

let isFiltering = false

// Danh sách chart
const chartKeys = ["accX", "accY", "accZ", "gyroX", "gyroY", "gyroZ", "temp"];

// Mỗi chart có 4 dataset (Node1-4)
window.fullData = {};
window.charts = {};

const colors = ["red", "blue", "green", "purple"];

// Hàm tạo chart
function createChart(ctx, label) {
  return new Chart(ctx, {
    type: "line",
    data: {
      datasets: [
        { label: "Node1 " + label, borderColor: colors[0], borderWidth: 1, pointRadius: 0, data: [], fill: false },
        { label: "Node2 " + label, borderColor: colors[1], borderWidth: 1, pointRadius: 0, data: [], fill: false },
        { label: "Node3 " + label, borderColor: colors[2], borderWidth: 1, pointRadius: 0, data: [], fill: false },
        { label: "Node4 " + label, borderColor: colors[3], borderWidth: 1, pointRadius: 0, data: [], fill: false }
      ]
    },
    options: {
      responsive: true,
      animation: false,
      interaction: {
        // mode: "nearest",   // Lấy điểm gần chuột
        mode: "index",
        intersect: false
      },
      plugins: {
        tooltip: {
          enabled: true,
          // mode: "nearest",
          mode: "index",
          intersect: false,
          callbacks: {
            label: function(context) {
              let label = context.dataset.label || "";
              let value = context.parsed.y;
              let time = new Date(context.parsed.x).toLocaleTimeString();
              return `${label}: ${value} (${time})`;
            }
          }
        },
        zoom: {
          zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: "x" },
          pan: { enabled: true, mode: "x" }
        }
      },
      scales: {
        x: {
          type: "time",
          time: { unit: "second" }
        },
        y: {
          beginAtZero: true
        }
      }
    }
  });
}

// Khởi tạo tất cả chart
window.addEventListener("DOMContentLoaded", () => {
  /* Start begin Y = 0, need to config in "scales" */
  chartKeys.forEach(key => {
    fullData[key] = [[], [], [], []]; // 4 node
    let ctx = document.getElementById("chart" + key.charAt(0).toUpperCase() + key.slice(1)).getContext("2d");
    charts[key] = createChart(ctx, key);
  });
});

// ========== FILTER / RESET ==========

function filterData(chartKey) {

  charts[chartKey].resetZoom();
  charts[chartKey].update("none");
  
  let start = document.getElementById("start" + chartKey.charAt(0).toUpperCase() + chartKey.slice(1)).value;
  let end = document.getElementById("end" + chartKey.charAt(0).toUpperCase() + chartKey.slice(1)).value;
  if (!start || !end) return;

  isFiltering = true;

  let today = new Date();
  let [sh, sm] = start.split(":");
  let [eh, em] = end.split(":");
  let startDate = new Date(today); startDate.setHours(sh, sm, 0, 0);
  let endDate = new Date(today); endDate.setHours(eh, em, 0, 0);

  fullData[chartKey].forEach((arr, idx) => {
    charts[chartKey].data.datasets[idx].data = arr.filter(p => p.x >= startDate && p.x <= endDate);
  });
  charts[chartKey].update();
}

function resetData(chartKey) {
  isFiltering = false;

  fullData[chartKey].forEach((arr, idx) => {
    let startIndex = Math.max(0, arr.length - MAX_POINTS);
    charts[chartKey].data.datasets[idx].data = arr.slice(startIndex);
  });
  charts[chartKey].resetZoom();
  charts[chartKey].update("none");
}

// Cho MQTT gọi update
window.updateChart = function (chartKey, nodeIndex, value) {
  let now = new Date();
  let point = { x: now, y: value };

  fullData[chartKey][nodeIndex].push(point);
  if (fullData[chartKey][nodeIndex].length > MAX_POINTS) {
    fullData[chartKey][nodeIndex].shift();
  }

  charts[chartKey].data.datasets[nodeIndex].data = fullData[chartKey][nodeIndex];
  charts[chartKey].update("none");
};
