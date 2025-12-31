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
/* // Tạo chart mẫu (cũ) & auto cập nhật mỗi giây bằng dữ liệu giả
renderRealtimeChart();
setInterval(() => {
  const sample = generateAccelSample(); // 1. lấy dữ liệu mới
  updateAccelChart(sample);             // 2. update chart
}, 1000); */

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

/* Giả lập data để hiển thị tổng hợp */
const summaryIMURowsMock = [
  { time: '14:03:30 31/08/2025', name_node: 1, ax: 0.035, ay: -0.012, az: 0.985, gx:  3.1, gy: -2.4, gz:  1.0, temp: 28.6 },
  { time: '14:02:30 31/08/2025', name_node: 2, ax: 0.081, ay:  0.020, az: 0.990, gx:  6.8, gy:  4.1, gz: -3.0, temp: 28.4 },
  { time: '14:01:30 31/08/2025', name_node: 3, ax: 0.022, ay: -0.018, az: 0.978, gx: -1.2, gy:  0.9, gz:  2.5, temp: 28.3 },
  { time: '14:00:30 31/08/2025', name_node: 1, ax: 0.029, ay:  0.010, az: 0.982, gx:  0.5, gy: -1.5, gz:  0.7, temp: 28.3 },
  { time: '13:59:30 31/08/2025', name_node: 2, ax: 0.110, ay:  0.035, az: 0.995, gx:  9.8, gy:  6.2, gz: -5.1, temp: 28.2 },
  { time: '13:58:30 31/08/2025', name_node: 3, ax: 0.040, ay: -0.022, az: 0.979, gx: -2.0, gy:  1.0, gz:  4.2, temp: 28.2 },
  { time: '13:57:30 31/08/2025', name_node: 1, ax: 0.033, ay:  0.014, az: 0.981, gx:  1.8, gy: -0.7, gz:  2.0, temp: 28.1 },
  { time: '13:56:30 31/08/2025', name_node: 2, ax: 0.025, ay: -0.011, az: 0.980, gx: -0.9, gy:  0.3, gz:  1.1, temp: 28.1 },
];

/* Định dạng số an toàn */
function fmt(v, nd=2) {
  if (v === null || v === undefined || Number.isNaN(v)) return '—';
  return Number(v).toFixed(nd);
}

function renderSummaryIMUTable(rows = summaryIMURowsMock) {
  const body = document.getElementById('summary-table-body');
  if (!body) return;

  body.innerHTML = rows.map(r => `
    <div class="table-row">
      <div>${r.time}</div>
      <div>${fmt(r.name_node,0)}</div>
      <div>${fmt(r.ax, 3)}</div>
      <div>${fmt(r.ay, 3)}</div>
      <div>${fmt(r.az, 3)}</div>
      <div>${fmt(r.gx, 1)}</div>
      <div>${fmt(r.gy, 1)}</div>
      <div>${fmt(r.gz, 1)}</div>
      <div>${fmt(r.temp, 1)}</div>
    </div>
  `).join('');
}

/* Export CSV (Excel mở trực tiếp) */
function exportSummaryIMUCSV(rows = summaryIMURowsMock) {
  const header = ['Thời gian', 'Node', 'Ax (g)','Ay (g)','Az (g)','Gx (°/s)','Gy (°/s)','Gz (°/s)','Nhiệt độ (°C)'];
  const lines = [header.join(',')].concat(
    rows.map(r => [r.time, r.name_node, r.ax, r.ay, r.az, r.gx, r.gy, r.gz, r.temp].join(','))
  );
  const csv = lines.join('\n');
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `TongHop_IMU_${new Date().toISOString().slice(0,19).replace(/[:T]/g,'-')}.csv`;
  a.click();
  URL.revokeObjectURL(url);
}


document.addEventListener('DOMContentLoaded', () => {
  renderSummaryIMUTable(summaryIMURowsMock);

  const btn = document.getElementById('btn-export-summary');
  if (btn) btn.addEventListener('click', () => exportSummaryIMUCSV());

  const sel = document.getElementById('summary-device');
  if (sel) sel.addEventListener('change', () => {
    // TODO: lọc theo node nếu có dữ liệu thật
    renderSummaryIMUTable(summaryIMURowsMock);
  });
});



document.addEventListener('DOMContentLoaded', () => {
  renderRealtimeChart(); // creates the 3 charts
});
const NODE_CONTAINER_IDS = [
  "chart-realtime-acc-node1",
  "chart-realtime-acc-node2",
  "chart-realtime-acc-node3"
];
function getChartByNodeIndex(nodeIndex) {
  const targetId = NODE_CONTAINER_IDS[Math.max(0, Math.min(2, nodeIndex))];
  return Highcharts.charts.find(c => c && c.renderTo && c.renderTo.id === targetId) || null;
}

const MAX_POINTS = 500;

// chartKey: 'accX'|'accY'|'accZ'|'gyroX'|'gyroY'|'gyroZ'
// nodeIndex: 0-based (N01->0, N02->1, N03->2)
// value: số
window.updateChart = function(chartKey, nodeIndex, value) {
  
  console.log("[UPDATE]", chartKey, "node", nodeIndex, "val", value);

  const chart = getChartByNodeIndex(nodeIndex);
  if (!chart) return;

  const seriesMap = { accX:0, accY:1, accZ:2, gyroX:3, gyroY:4, gyroZ:5 };
  const sIdx = seriesMap[chartKey];
  if (sIdx === undefined) return;

  chart.series[sIdx].addPoint(value, false);

  // Nhãn thời gian: dùng local time (nếu muốn dùng ts từ JSON, có thể sửa để nhận timeLabel)
  const timeLabel = new Date().toLocaleTimeString();
  chart.xAxis[0].categories.push(timeLabel);

  // Giới hạn điểm để giữ hiệu năng
  if (chart.series[sIdx].data.length > MAX_POINTS) chart.series[sIdx].removePoint(0, false);
  if (chart.xAxis[0].categories.length > MAX_POINTS) chart.xAxis[0].categories.shift();

  chart.redraw();
};

