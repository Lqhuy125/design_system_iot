const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: {
    datasets: [
      {label: 'Node1 ax', data: [], borderColor: 'red', fill: false, borderWidth: 1, pointRadius: 0},
      {label: 'Node2 ax', data: [], borderColor: 'blue', fill: false, borderWidth: 1, pointRadius: 0},
      {label: 'Node3 ax', data: [], borderColor: 'green', fill: false, borderWidth: 1, pointRadius: 0},
      {label: 'Node4 ax', data: [], borderColor: 'purple', fill: false, borderWidth: 1, pointRadius: 0}
    ]
  },
  options: {
    responsive: true,
    animation: false,
    interaction: { mode: 'nearest', intersect: false },
    plugins: { 
      tooltip: { enabled: true },
      zoom: {
        zoom: {
          wheel: { enabled: true },
          pinch: { enabled: true },
          mode: 'x'
        },
        pan: { enabled: true, mode: 'x' }
      }
    },
    scales: {
      x: { type: 'time', time: { unit: 'second' }, display: true },
      y: { beginAtZero: true }
    }
  }
});

/* Array to save all data for filtering */
let fullData = {
  datasets: [[], [], [], []]  // mỗi dataset lưu {x,y}
};

// 🔹 Giữ tối đa N điểm để chart không bị nặng
const MAX_POINTS = 10; 
let isFiltering = false;

function filterData() {
  chart.resetZoom();

  let start = document.getElementById("startTime").value;
  let end = document.getElementById("endTime").value;
  if (!start || !end) return;

  isFiltering = true;

  let today = new Date();
  let [sh, sm] = start.split(":");
  let [eh, em] = end.split(":");

  let startDate = new Date(today);
  startDate.setHours(parseInt(sh), parseInt(sm), 0, 0);

  let endDate = new Date(today);
  endDate.setHours(parseInt(eh), parseInt(em), 59, 999);

  // lọc trực tiếp theo {x,y}
  for (let i = 0; i < 4; i++) {
    chart.data.datasets[i].data = fullData.datasets[i].filter(
      p => p.x >= startDate && p.x <= endDate
    );
  }

  chart.update();
}

function resetData() {
  isFiltering = false;

  for (let i = 0; i < 4; i++) {
    let data = fullData.datasets[i];
    let startIndex = Math.max(0, data.length - MAX_POINTS);
    chart.data.datasets[i].data = data.slice(startIndex);
  }

  chart.resetZoom();
  chart.update('none');
}
