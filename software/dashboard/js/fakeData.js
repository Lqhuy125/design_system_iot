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

  setInterval(addFakeData, 100);