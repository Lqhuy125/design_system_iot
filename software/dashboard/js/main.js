const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      {label: 'Node1 ax', data: [], borderColor: 'red', fill: false, borderWidth: 1, pointRadius: 0,},
      {label: 'Node2 ax', data: [], borderColor: 'blue', fill: false, borderWidth: 1, pointRadius: 0,},
      {label: 'Node3 ax', data: [], borderColor: 'green', fill: false, borderWidth: 1, pointRadius: 0,},
      {label: 'Node4 ax', data: [], borderColor: 'purple', fill: false, borderWidth: 1, pointRadius: 0,}
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
              wheel: {
                enabled: true // bật zoom bằng lăn chuột
              },
              pinch: {
                enabled: true // bật zoom bằng cảm ứng
              },
              mode: 'x', // chỉ zoom theo trục X
            },
            pan: {
              enabled: true, // bật pan (kéo qua lại)
              mode: 'x'
            }
          }
    
    },
    scales: { x: {display: true}, y: {beginAtZero: true} }
  }
});

// const bigCtx = document.getElementById('bigChart').getContext('2d');
// const bigChart = new Chart(bigCtx, {
//   type: 'line',
//   data: chart.data, /* "data" can not use for many chart */
//   options: chart.options
// });

/* Array to save all data for filtering */
let fullData = {
    labels: [],
    datasets: [[], [], [], []]  // mỗi dataset lưu riêng
  };

// 🔹 Giữ tối đa N điểm để chart không bị nặng
const MAX_POINTS = 100; 

let isFiltering = false;
  function addFakeData() {
    let time = new Date();
    let timeLabel = time.toLocaleTimeString();
  
    // luôn lưu dữ liệu đầy đủ
    fullData.labels.push(time);
    for (let i = 0; i < 4; i++) {
      let fakeValue = (Math.sin(Date.now()/1000 + i) * 5 + Math.random()*2).toFixed(2);
      fullData.datasets[i].push({ t: time, v: fakeValue });
    }
  
    // nếu đang filter thì không update chart
    if (isFiltering) return;
  
    // nếu không filter thì update chart realtime
    chart.data.labels.push(timeLabel);
    for (let i = 0; i < 4; i++) {
      chart.data.datasets[i].data.push(fullData.datasets[i][fullData.datasets[i].length - 1].v);
    }

     
    if (chart.data.labels.length > MAX_POINTS) {
        chart.data.labels.shift(); // bỏ điểm đầu
        for (let i = 0; i < 4; i++) {
            chart.data.datasets[i].data.shift();
        }
    }
  
    chart.update();
    //   bigChart.update();
  }
  

function filterData() {
    /* Clear zoom in or zoom out when filter */
    chart.resetZoom();

    let start = document.getElementById("startTime").value;
    let end = document.getElementById("endTime").value;
    if (!start || !end) return;
  
    isFiltering = true;  // bật cờ filter
  
    let today = new Date().toDateString();
    let startDate = new Date(today + " " + start);
    let endDate = new Date(today + " " + end);
  
    chart.data.labels = [];
    for (let i = 0; i < 4; i++) {
      chart.data.datasets[i].data = [];
    }
  
    fullData.labels.forEach((time, idx) => {
      if (time >= startDate && time <= endDate) {
        chart.data.labels.push(time.toLocaleTimeString());
        for (let i = 0; i < 4; i++) {
          chart.data.datasets[i].data.push(fullData.datasets[i][idx].v);
        }
      }
    });
  
    chart.update();
  }
  

  function resetData() {
    isFiltering = false; // tắt filter
  
    // lấy số lượng dữ liệu gần nhất (nếu nhiều hơn MAX_POINTS thì cắt bớt ở đầu)
    let startIndex = Math.max(0, fullData.labels.length - MAX_POINTS);

    chart.data.labels = fullData.labels.slice(startIndex).map(t => t.toLocaleTimeString());

    for (let i = 0; i < 4; i++) {
        chart.data.datasets[i].data = fullData.datasets[i]
        .slice(startIndex)
        .map(obj => obj.v);
    }
    chart.update();
    chart.resetZoom();
  }
  

setInterval(addFakeData, 100);
