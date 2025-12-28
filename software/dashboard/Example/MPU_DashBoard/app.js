/*================START INFOMATION NODE LIST================= */

// --- MOCK: 2 node ví dụ cho Khu vực 1 ---
const mockNodes = [
  { id:"N01", name:"Vị trí 1", areaId:1, span:"A",  status:"online",  battery:92, temp:28.4 },
  { id:"N02", name:"Vị trí 2", areaId:1, span:"T1", status:"offline", battery:67, temp:29.1 },
  { id:"N02", name:"Vị trí 3", areaId:1, span:"T1", status:"offline", battery:67, temp:29.1 },
];

/* Lấy danh sách của khu vực
=> lọc những node có areaId = 1 */
function getArea1Nodes()
{ 
  return mockNodes.filter(n => n.areaId === 1); 
}

function renderArea1Table(){
  const nodes = getArea1Nodes();
  const body  = document.getElementById('area-1-table-body');
  if (!body) return;
  body.innerHTML = nodes.map((n, idx) => `
    <div class="table-row">
      <div>${idx+1}</div>
      <div>${n.id}</div>
      <div>${n.name}</div>
      <div>${n.span}</div>
      <div class="table-status">${n.status==='online'?'Online':'Offline'}</div>
      <div class="table-sub">${n.battery>0 ? (n.battery + '%') : '—'}</div>
      <div class="table-sub">${(n.temp ?? '—') + (n.temp ? ' °C' : '')}</div>
    </div>
  `).join('');
}

// Demo refresh button toggles statuses randomly
const btnRefresh = () => {
  const btn = document.getElementById('btn-area-1-table-refresh');
  if (!btn) return;
  btn.addEventListener('click', ()=>{
    getArea1Nodes().forEach(n=>{ 
      if(Math.random()<0.35) n.status = (n.status==='online')?'offline':'online'; 
      n.battery = Math.floor(Math.random() * (100 - 1 + 1) + 1);
      n.temp = Math.floor(Math.random() * (50 - 18 + 1) + 18);
    });
    renderArea1Table();
  });
};

// Init
renderArea1Table();
btnRefresh();
/*================END INFOMATION NODE LIST================= */

/*================START DISLAY CHART ACC================= */
/* Hiện thị chart cho Accelerator các node - Cả 3 trục của 1 node trên 1 chart */
function renderRealtimeChart() {
  const chart_acc_node1 = Highcharts.chart('chart-realtime-acc-node1', {
    chart: { type:'line' },
    title: { text: 'Dữ liệu rung động (Gia tốc & Tốc độ góc)' },
    xAxis: {
      categories: [],
      title: { text: 'Thời điểm' }
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Gia tốc (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Tốc độ góc (°/s)' },
        opposite: true,
        min: -10,
        max: 10
      }
    ],
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    series: [
      { name: 'Ax', data: [] },
      { name: 'Ay', data: [] },
      { name: 'Az', data: [] },
      { name: 'Gx', yAxis: 1, data: [] },
      { name: 'Gy', yAxis: 1, data: [] },
      { name: 'Gz', yAxis: 1, data: [] }
    ]
  });

  const chart_acc_node2 = Highcharts.chart('chart-realtime-acc-node2', {
    chart: { type:'line' },
    title: { text: 'Dữ liệu rung động (Gia tốc & Tốc độ góc)' },
    xAxis: {
      categories: [],
      title: { text: 'Thời điểm' }
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Gia tốc (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Tốc độ góc (°/s)' },
        opposite: true,
        min: -10,
        max: 10
      }
    ],
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    series: [
      { name: 'Ax', data: [] },
      { name: 'Ay', data: [] },
      { name: 'Az', data: [] },
      { name: 'Gx', yAxis: 1, data: [] },
      { name: 'Gy', yAxis: 1, data: [] },
      { name: 'Gz', yAxis: 1, data: [] }
    ]
  });

  const chart_acc_node3 = Highcharts.chart('chart-realtime-acc-node3', {
    chart: { type:'line' },
    title: { text: 'Dữ liệu rung động (Gia tốc & Tốc độ góc)' },
    xAxis: {
      categories: [],
      title: { text: 'Thời điểm' }
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Gia tốc (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Tốc độ góc (°/s)' },
        opposite: true,
        min: -10,
        max: 10
      }
    ],
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    series: [
      { name: 'Ax', data: [] },
      { name: 'Ay', data: [] },
      { name: 'Az', data: [] },
      { name: 'Gx', yAxis: 1, data: [] },
      { name: 'Gy', yAxis: 1, data: [] },
      { name: 'Gz', yAxis: 1, data: [] }
    ]
  });

  /* Chức năng ẩn/hiện theo loại data */
  addEyeToggle(chart_acc_node1);
  addEyeToggle(chart_acc_node2);
  addEyeToggle(chart_acc_node3);
}

function updateAccelChart(sample) {
  /* Hiển thị Acc cho từng node */
  const chart_acc_node1 = Highcharts.charts[0];

  chart_acc_node1.xAxis[0].categories.push(sample.time);
  chart_acc_node1.series[0].addPoint(sample.ax, false);
  chart_acc_node1.series[1].addPoint(sample.ay, false);
  chart_acc_node1.series[2].addPoint(sample.az, false);
  
  chart_acc_node1.series[3].addPoint(sample.gx, false);
  chart_acc_node1.series[4].addPoint(sample.gy, false);
  chart_acc_node1.series[5].addPoint(sample.gz, false);

  const chart_acc_node2 = Highcharts.charts[1];

  chart_acc_node2.xAxis[0].categories.push(sample.time);
  chart_acc_node2.series[0].addPoint(sample.ax, false);
  chart_acc_node2.series[1].addPoint(sample.ay, false);
  chart_acc_node2.series[2].addPoint(sample.az, false);
  chart_acc_node2.series[3].addPoint(sample.gx, false);
  chart_acc_node2.series[4].addPoint(sample.gy, false);
  chart_acc_node2.series[5].addPoint(sample.gz, false);

  const chart_acc_node3 = Highcharts.charts[2];

  chart_acc_node3.xAxis[0].categories.push(sample.time);
  chart_acc_node3.series[0].addPoint(sample.ax, false);
  chart_acc_node3.series[1].addPoint(sample.ay, false);
  chart_acc_node3.series[2].addPoint(sample.az, false);
  chart_acc_node3.series[3].addPoint(sample.gx, false);
  chart_acc_node3.series[4].addPoint(sample.gy, false);
  chart_acc_node3.series[5].addPoint(sample.gz, false);

  chart_acc_node1.redraw();
  chart_acc_node2.redraw();
  chart_acc_node3.redraw();

  /* Hiện thị Gyro cho từng node */
  
}
function generateAccelSample() {
  const ax = +(Math.random() * 0.2 - 0.1).toFixed(3);
  const ay = +(Math.random() * 0.2 - 0.1).toFixed(3);
  const az = +(0.98 + Math.random() * 0.05).toFixed(3);

  const gx = +(Math.random()*20 - 10).toFixed(1);
  const gy = +(Math.random()*20 - 10).toFixed(1);
  const gz = +(Math.random()*20 - 10).toFixed(1);

  return {
    time: new Date().toLocaleTimeString(),
    ax,
    ay,
    az,
    gx,
    gy,
    gz
  };
}

renderRealtimeChart();
setInterval(() => {
  const sample = generateAccelSample(); // 1. lấy dữ liệu mới
  updateAccelChart(sample);             // 2. update chart
}, 1000);

function addEyeToggle(chart) {
  let accVisible = true;
  let gyroVisible = true;

  // ◉ ACC
  const accEye = chart.renderer
    .label('◉ ACC', 10, chart.chartHeight - 35)
    .css({ cursor: 'pointer', fontSize: '12px' })
    .attr({ zIndex: 7 })
    .add();

  accEye.on('click', function () {
    accVisible = !accVisible;
    chart.series.forEach(s => {
      if (['Ax', 'Ay', 'Az'].includes(s.name)) {
        s.setVisible(accVisible, false);
      }
    });
    chart.redraw();
    accEye.attr({ text: accVisible ? '◉ ACC' : '🚫 ACC' });
  });

  // ◉ GYRO
  const gyroEye = chart.renderer
    .label('◉ GYRO', 80, chart.chartHeight - 35)
    .css({ cursor: 'pointer', fontSize: '12px' })
    .attr({ zIndex: 7 })
    .add();

  gyroEye.on('click', function () {
    gyroVisible = !gyroVisible;
    chart.series.forEach(s => {
      if (['Gx', 'Gy', 'Gz'].includes(s.name)) {
        s.setVisible(gyroVisible, false);
      }
    });
    chart.redraw();
    gyroEye.attr({ text: gyroVisible ? '◉ GYRO' : '🚫 GYRO' });
  });
}
/*================END DISLAY CHART ACC================= */

/* Thu nhỏ char */
function toggleNode(nodeId, arrow) {
  const node = document.getElementById(nodeId);

  if (!node) return;

  const isHidden = node.style.display === "none";

  node.style.display = isHidden ? "block" : "none";

  arrow.classList.toggle("open", isHidden);
}
/* ======================================= */