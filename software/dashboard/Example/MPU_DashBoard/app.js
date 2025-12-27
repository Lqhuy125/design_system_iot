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
  const chart_node1 = Highcharts.chart('chart-realtime-acc-node1', {
    chart: { type:'line' },
    title: { text: 'Gia tốc – Node 01' },
    xAxis: {
      categories: [],
      title: { text: 'Thời điểm' }
    },
    yAxis: { title: { text: 'Gia tốc (g)' } },
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    series: [
      { name: 'Ax', data: [] },
      { name: 'Ay', data: [] },
      { name: 'Az', data: [] }
    ]
  });

  const chart_node2 = Highcharts.chart('chart-realtime-acc-node2', {
    chart: { type:'line' },
    title: { text: 'Gia tốc – Node 02' },
    xAxis: {
      categories: [],
      title: { text: 'Thời điểm' }
    },
    yAxis: { title: { text: 'Gia tốc (g)' } },
    legend: { enabled:true },
    credits: { enabled:false },
    exporting:{ enabled:true },
    series: [
      { name: 'Ax', data: [] },
      { name: 'Ay', data: [] },
      { name: 'Az', data: [] }
    ]
  });
}

function updateAccelChart(sample) {
  const chart_node1 = Highcharts.charts[0];

  chart_node1.xAxis[0].categories.push(sample.time);
  chart_node1.series[0].addPoint(sample.ax, false);
  chart_node1.series[1].addPoint(sample.ay, false);
  chart_node1.series[2].addPoint(sample.az, false);

  const chart_node2 = Highcharts.charts[1];

  chart_node2.xAxis[0].categories.push(sample.time);
  chart_node2.series[0].addPoint(sample.ax, false);
  chart_node2.series[1].addPoint(sample.ay, false);
  chart_node2.series[2].addPoint(sample.az, false);

  chart_node1.redraw();
  chart_node2.redraw();
}
function generateAccelSample() {
  const ax = +(Math.random() * 0.2 - 0.1).toFixed(3);
  const ay = +(Math.random() * 0.2 - 0.1).toFixed(3);
  const az = +(0.98 + Math.random() * 0.05).toFixed(3);

  return {
    time: new Date().toLocaleTimeString(),
    ax,
    ay,
    az
  };
}

renderRealtimeChart();
setInterval(() => {
  const sample = generateAccelSample(); // 1. lấy dữ liệu mới
  updateAccelChart(sample);             // 2. update chart
}, 1000);

/*================END DISLAY CHART ACC================= */