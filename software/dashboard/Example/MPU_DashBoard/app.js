/*================START INFOMATION NODE LIST================= */

// --- MOCK: 2 node ví dụ cho Area 1 ---
const mockNodes = [
  { id:"N01", name:"Location 1", areaId:1, span:"A",  status:"online"},
  { id:"N02", name:"Location 2", areaId:1, span:"T1", status:"online"},
  { id:"N03", name:"Location 3", areaId:1, span:"T1", status:"online"},
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
/* Display accelerometer charts for nodes - all 3 axes on one chart */
function renderRealtimeChart() {
  const chart_acc_node1 = Highcharts.chart('chart-realtime-acc-node1', {
    chart: { type:'line' },
    title: { text: 'Vibration data (Acceleration & Angular velocity)' },
    xAxis: {
      type: 'datetime',
      categories: [],
      title: { text: 'Timestamp' },
      tickPixelInterval: 100,  
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Acceleration (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Angular velocity (°/s)' },
        opposite: true,
        min: -10,
        max: 10
      }
    ],
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    tooltip: {
      // Tooltip shows HH:mm (you can add seconds if needed)
      xDateFormat: '%H:%M',       // chỉ giờ:phút
      shared: true
    },
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
    title: { text: 'Vibration data (Acceleration & Angular velocity)' },
    xAxis: {
      categories: [],
      title: { text: 'Timestamp' }
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Acceleration (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Angular velocity (°/s)' },
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
    title: { text: 'Vibration data (Acceleration & Angular velocity)' },
    xAxis: {
      categories: [],
      title: { text: 'Timestamp' }
    },
    yAxis: [
      { // Y LEFT – ACC
        title: { text: 'Acceleration (g)' },
        min: -2,
        max: 2
      },
      { // Y RIGHT – GYRO
        title: { text: 'Angular velocity (°/s)' },
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

const MAX_POINTS = 50 ;

window.updateChart = function(chartKey, nodeIndex, value) {
  
  // console.log("[UPDATE]", chartKey, "node", nodeIndex, "val", value);

  const chart = getChartByNodeIndex(nodeIndex);
  if (!chart) return;

  const seriesMap = { accX:0, accY:1, accZ:2, gyroX:3, gyroY:4, gyroZ:5 };
  const sIdx = seriesMap[chartKey];
  if (sIdx === undefined) return;

  chart.series[sIdx].addPoint(value, false);

  // Nhãn thời gian: dùng local time (nếu muốn dùng ts từ JSON, có thể sửa để nhận timeLabel)
  // const timeLabel = new Date().toLocaleTimeString('vi-VN', {
  //   hour: '2-digit',
  //   minute: '2-digit'
  // });

  const timeLabel = new Date().toLocaleTimeString();
  const categories = chart.xAxis[0].categories;
  if (categories[categories.length - 1] !== timeLabel) {
    chart.xAxis[0].categories.push(timeLabel);
  }
  else
  {
    chart.xAxis[0].categories.push('');
  }
  // Limit points to keep performance
  if (chart.series[sIdx].data.length > MAX_POINTS) chart.series[sIdx].removePoint(0, false);
  // if (chart.xAxis[0].categories.length > MAX_POINTS) chart.xAxis[0].categories.shift();

  chart.redraw();
};

/* Toggle ACC and GYRO */
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

/* Collapse chart */
function toggleNode(nodeId, arrow) {
  const node = document.getElementById(nodeId);

  if (!node) return;

  const isHidden = node.style.display === "none";

  node.style.display = isHidden ? "block" : "none";

  arrow.classList.toggle("open", isHidden);
}
/* ======================================= */

/*================SUMMARY REPORT================= 
      Mock data for summary display 
=============================================== */
/* const summaryIMURowsMock = [
  { time: '14:03:30 31/08/2025', name_node: 1, ax: 0.035, ay: -0.012, az: 0.985, gx:  3.1, gy: -2.4, gz:  1.0, temp: 28.6 },
  { time: '14:02:30 31/08/2025', name_node: 2, ax: 0.081, ay:  0.020, az: 0.990, gx:  6.8, gy:  4.1, gz: -3.0, temp: 28.4 },
  { time: '14:01:30 31/08/2025', name_node: 3, ax: 0.022, ay: -0.018, az: 0.978, gx: -1.2, gy:  0.9, gz:  2.5, temp: 28.3 },
  { time: '14:00:30 31/08/2025', name_node: 1, ax: 0.029, ay:  0.010, az: 0.982, gx:  0.5, gy: -1.5, gz:  0.7, temp: 28.3 },
  { time: '13:59:30 31/08/2025', name_node: 2, ax: 0.110, ay:  0.035, az: 0.995, gx:  9.8, gy:  6.2, gz: -5.1, temp: 28.2 },
  { time: '13:58:30 31/08/2025', name_node: 3, ax: 0.040, ay: -0.022, az: 0.979, gx: -2.0, gy:  1.0, gz:  4.2, temp: 28.2 },
  { time: '13:57:30 31/08/2025', name_node: 1, ax: 0.033, ay:  0.014, az: 0.981, gx:  1.8, gy: -0.7, gz:  2.0, temp: 28.1 },
  { time: '13:56:30 31/08/2025', name_node: 2, ax: 0.025, ay: -0.011, az: 0.980, gx: -0.9, gy:  0.3, gz:  1.1, temp: 28.1 },
]; */

/* Safe number formatting */
function fmtSafe(v, nd=2) {
  if (v === null || v === undefined || Number.isNaN(v)) return '—';
  return Number(v).toFixed(nd);
}

/* Export CSV (Excel can open directly) */
function exportSummaryIMUCSV(rows = summaryIMURowsMock) {
  const header = ['Time', 'Node', 'Ax (g)','Ay (g)','Az (g)','Gx (°/s)','Gy (°/s)','Gz (°/s)','Temperature (°C)'];
  const lines = [header.join(',')].concat(
    rows.map(r => [r.time, r.name_node, r.ax, r.ay, r.az, r.gx, r.gy, r.gz, r.temp].join(','))
  );
  const csv = lines.join('\n');
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `IMU_Summary_${new Date().toISOString().slice(0,19).replace(/[:T]/g,'-')}.csv`;
  a.click();
  URL.revokeObjectURL(url);
}

window.appendSummaryRow = function(sample) {
  const body = document.getElementById('summary-table-body');
  if (!body) return;

  // Nhãn thời gian: ưu tiên ts từ JSON; fallback: local time
  const timeLabel = new Date().toLocaleString();

  // Chuẩn hóa node hiển thị
  const nodeLabel =
    typeof sample.nodeId === 'string'
      ? sample.nodeId
      : `N${String(sample.nodeId).padStart(2, '0')}`;

  const tempCell = (typeof sample.temp === 'number')
      ? fmtSafe(sample.temp, 1)
      : '—';
  
  // Tạo 1 dòng theo layout table của bạn (thứ tự cột: Time, Node, Ax, Ay, Az, Gx, Gy, Gz, Temp)
  const row = document.createElement('div');
  row.className = 'table-row';
  row.innerHTML = `
    <div>${timeLabel}</div>
    <div>${nodeLabel}</div>
    <div>${fmtSafe(sample.ax, 3)}</div>
    <div>${fmtSafe(sample.ay, 3)}</div>
    <div>${fmtSafe(sample.az, 3)}</div>
    <div>${fmtSafe(sample.gx, 1)}</div>
    <div>${fmtSafe(sample.gy, 1)}</div>
    <div>${fmtSafe(sample.gz, 1)}</div>
    <div>${fmtSafe(sample.temp, 1)}</div>
    <div></div>
  `;
  // Thêm lên đầu (realtime mới nhất ở trên)
  body.prepend(row);

  // (Tuỳ chọn) Giới hạn số dòng để không quá dài
  const MAX_ROWS = 10;
  while (body.children.length > MAX_ROWS) {
    body.removeChild(body.lastElementChild);
  }
};


