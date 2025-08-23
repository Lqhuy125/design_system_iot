const ctx = document.getElementById('chart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      {label: 'Node1 ax', data: [], borderColor: 'red', fill: false},
      {label: 'Node2 ax', data: [], borderColor: 'blue', fill: false},
      {label: 'Node3 ax', data: [], borderColor: 'green', fill: false},
      {label: 'Node4 ax', data: [], borderColor: 'purple', fill: false}
    ]
  },
  options: {
    responsive: true,
    animation: false,
    interaction: { mode: 'nearest', intersect: false },
    plugins: { tooltip: { enabled: true } },
    scales: { x: {display: true}, y: {beginAtZero: true} }
  }
});

// const bigCtx = document.getElementById('bigChart').getContext('2d');
// const bigChart = new Chart(bigCtx, {
//   type: 'line',
//   data: chart.data, /* "data" can not use for many chart */
//   options: chart.options
// });

function addFakeData() {
  let time = new Date().toLocaleTimeString();

  if (chart.data.labels.length > 20) chart.data.labels.shift();
  chart.data.labels.push(time);

  for (let i = 0; i < 4; i++) {
    if (chart.data.datasets[i].data.length > 20) chart.data.datasets[i].data.shift();
    let fakeValue = (Math.sin(Date.now()/1000 + i) * 5 + Math.random()*2).toFixed(2);
    chart.data.datasets[i].data.push(fakeValue);
  }

  chart.update();
//   bigChart.update();
}

setInterval(addFakeData, 1000);
