'use strict';

const el = (id) => document.getElementById(id);
let csvText = '';
let activeReport = null;
let hoverIndex = -1;
let chartPoints = [];
const number = (v) => Number(v).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
const signed = (v) => `${v >= 0 ? '+' : ''}${number(v)}%`;

function showError(message) {
  const element = el('errorBox');
  element.textContent = message;
  element.hidden = false;
}
function clearError() { el('errorBox').hidden = true; }
function setData(text, filename, label) {
  csvText = text;
  el('fileName').textContent = filename;
  el('fileStatus').textContent = label;
  clearError();
}

async function loadDemo() {
  clearError();
  try {
    const response = await fetch('/sample.csv');
    if (!response.ok) throw new Error('Sample CSV could not be loaded.');
    setData(await response.text(), 'sample_stock.csv', 'READY');
    await analyze();
  } catch (error) { showError(error.message); }
}

async function handleFile(file) {
  if (!file) return;
  if (!file.name.toLowerCase().endsWith('.csv')) {
    showError('Please select a .csv file with Date and Close columns.');
    return;
  }
  if (file.size > 2 * 1024 * 1024) {
    showError('The maximum CSV file size is 2 MiB.');
    return;
  }
  setData(await file.text(), file.name, 'READY');
  await analyze();
}

async function analyze() {
  if (!csvText) {
    showError('Please upload a CSV file or load the example dataset.');
    return;
  }
  clearError();
  const button = el('analyzeBtn');
  button.disabled = true;
  button.textContent = 'Analyzing with C++…';
  try {
    const windowSize = Number(el('windowInput').value);
    const horizon = Number(el('horizonInput').value);
    const response = await fetch(`/api/analyze?window=${windowSize}&days=${horizon}`, {
      method: 'POST',
      headers: { 'Content-Type': 'text/csv; charset=utf-8' },
      body: csvText
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || `Server returned ${response.status}`);
    activeReport = result;
    hoverIndex = -1;
    renderAll();
  } catch (error) {
    showError(error.message || 'The C++ server could not be reached. Is it running?');
  } finally {
    button.disabled = false;
    button.innerHTML = 'Run C++ analysis <span aria-hidden="true">→</span>';
  }
}

function renderAll() {
  const r = activeReport;
  if (!r) return;
  el('asOfDate').textContent = `As of ${r.history[r.history.length - 1].date}`;
  el('latestClose').textContent = number(r.latest_close);
  el('latestSma').textContent = number(r.latest_sma);
  el('historyCount').textContent = `${r.history.length} historical observations`;
  el('smaDetail').textContent = `${r.window}-observation SMA`;
  const trend = el('trend');
  trend.textContent = r.trend;
  trend.className = 'metric-trend ' + (r.trend === 'Upward' ? 'is-up' : r.trend === 'Downward' ? 'is-down' : 'is-flat');
  el('slopeDetail').textContent = `SMA slope ${r.slope >= 0 ? '+' : ''}${number(r.slope)}`;
  const change = el('dailyChange');
  change.textContent = signed(r.change_pct);
  change.className = r.change_pct >= 0 ? 'is-up' : 'is-down';
  renderTables();
  drawChart();
  el('downloadBtn').disabled = false;
}

function renderTables() {
  const r = activeReport;
  el('forecastTable').innerHTML = r.forecasts.map((f) => {
    const delta = 100 * (f.predicted_close / r.latest_close - 1);
    return `<tr><td>+${f.day}</td><td>${number(f.predicted_close)}</td><td class="${delta >= 0 ? 'positive' : 'negative'}">${signed(delta)}</td></tr>`;
  }).join('');
  el('historyTable').innerHTML = r.history.slice(-6).reverse().map((h) =>
    `<tr><td>${h.date}</td><td>${number(h.close)}</td><td>${h.sma === null ? '—' : number(h.sma)}</td></tr>`
  ).join('');
}

function downloadForecast() {
  if (!activeReport) return;
  const r = activeReport;
  const rows = ['FutureStep,EstimatedClose,ChangeVersusLastClosePercent', ...r.forecasts.map((f) =>
    `${f.day},${f.predicted_close.toFixed(4)},${(100 * (f.predicted_close / r.latest_close - 1)).toFixed(4)}`
  )];
  const blob = new Blob([rows.join('\r\n') + '\r\n'], { type: 'text/csv;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = 'stocklab_forecast.csv';
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function drawChart() {
  if (!activeReport) return;
  const canvas = el('priceChart');
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (width < 100 || height < 100) return;
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  canvas.width = Math.round(width * dpr);
  canvas.height = Math.round(height * dpr);
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);
  const r = activeReport;
  const n = r.history.length;
  const future = r.forecasts.length;
  const total = n + future;
  const margin = { left: 57, right: 14, top: 17, bottom: 36 };
  const plotW = width - margin.left - margin.right;
  const plotH = height - margin.top - margin.bottom;
  const prices = [...r.history.map(h => h.close),
    ...r.history.filter(h => h.sma !== null).map(h => h.sma),
    ...r.forecasts.map(f => f.predicted_close)];
  const min = Math.min(...prices), max = Math.max(...prices);
  const pad = Math.max((max - min) * 0.13, min * 0.005, 0.5);
  const yLow = Math.max(0, min - pad), yHigh = max + pad;
  const X = (i) => margin.left + i / (total - 1) * plotW;
  const Y = (price) => margin.top + (yHigh - price) / (yHigh - yLow) * plotH;
  const dividerX = X(n - 1);

  ctx.fillStyle = '#173a3d50';
  ctx.fillRect(dividerX, margin.top, width - margin.right - dividerX, plotH);
  ctx.font = '10px ui-sans-serif, system-ui, sans-serif';
  for (let i = 0; i <= 4; i++) {
    const y = margin.top + plotH / 4 * i;
    ctx.strokeStyle = '#32415870';
    ctx.lineWidth = 1;
    ctx.setLineDash([2, 5]);
    ctx.beginPath(); ctx.moveTo(margin.left, y); ctx.lineTo(width - margin.right, y); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#8492a8'; ctx.textAlign = 'right';
    ctx.fillText(number(yHigh - (yHigh - yLow) * i / 4), margin.left - 11, y + 4);
  }
  ctx.strokeStyle = '#7ad6c275';
  ctx.setLineDash([4, 5]);
  ctx.beginPath(); ctx.moveTo(dividerX, margin.top); ctx.lineTo(dividerX, margin.top + plotH); ctx.stroke();
  ctx.setLineDash([]);

  function pathFrom(values, color, lineWidth, dash = []) {
    ctx.beginPath(); ctx.strokeStyle = color; ctx.lineWidth = lineWidth;
    ctx.lineJoin = 'round'; ctx.lineCap = 'round'; ctx.setLineDash(dash);
    let started = false;
    values.forEach((point) => {
      if (point.value === null) return;
      if (!started) { ctx.moveTo(X(point.index), Y(point.value)); started = true; }
      else ctx.lineTo(X(point.index), Y(point.value));
    });
    ctx.stroke(); ctx.setLineDash([]);
  }
  pathFrom(r.history.map((h, i) => ({ index: i, value: h.close })), '#7ac5fa', 2.1);
  pathFrom(r.history.map((h, i) => ({ index: i, value: h.sma })), '#b399ff', 2.1);
  pathFrom([{ index: n - 1, value: r.latest_close }, ...r.forecasts.map((f) => ({ index: n - 1 + f.day, value: f.predicted_close }))], '#73e6c5', 2.5, [6, 4]);

  ctx.fillStyle = '#74e8c6';
  r.forecasts.forEach((f) => {
    ctx.beginPath(); ctx.arc(X(n - 1 + f.day), Y(f.predicted_close), 2.8, 0, 2 * Math.PI); ctx.fill();
  });
  ctx.fillStyle = '#bfe8ff';
  ctx.beginPath(); ctx.arc(X(n - 1), Y(r.latest_close), 3.6, 0, 2 * Math.PI); ctx.fill();

  const labelIndices = [0, Math.round((n - 1) / 3), Math.round((n - 1) * 2 / 3), n - 1, total - 1];
  ctx.textAlign = 'center'; ctx.font = '9px ui-sans-serif, system-ui, sans-serif';
  labelIndices.forEach((index, j) => {
    ctx.fillStyle = j === labelIndices.length - 1 ? '#79deca' : '#8b9bb2';
    const label = index >= n ? `+${index - n + 1}` : r.history[index].date.slice(5);
    ctx.fillText(label, X(index), height - 12);
  });
  chartPoints = [...r.history.map((h, i) => ({ index: i, label: h.date, value: h.close, type: 'Close' })),
    ...r.forecasts.map((f) => ({ index: n - 1 + f.day, label: `Step +${f.day}`, value: f.predicted_close, type: 'Forecast' }))];

  if (hoverIndex >= 0 && hoverIndex < chartPoints.length) {
    const point = chartPoints[hoverIndex];
    const px = X(point.index), py = Y(point.value);
    ctx.setLineDash([3, 4]); ctx.strokeStyle = '#9ebbd899'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(px, margin.top); ctx.lineTo(px, margin.top + plotH); ctx.stroke();
    ctx.setLineDash([]);
    ctx.beginPath(); ctx.fillStyle = '#e0fbff'; ctx.arc(px, py, 4.2, 0, 2 * Math.PI); ctx.fill();
    const tooltip = el('chartTooltip');
    tooltip.textContent = `${point.label} • ${point.type}: ${number(point.value)}`;
    tooltip.hidden = false;
    const tipX = Math.max(6, Math.min(width - tooltip.offsetWidth - 6, px + 12));
    tooltip.style.left = `${tipX}px`;
    tooltip.style.top = `${Math.max(8, Math.min(height - tooltip.offsetHeight - 7, py - 40))}px`;
  }
}

function onChartMove(event) {
  if (!activeReport) return;
  const canvas = el('priceChart');
  const rect = canvas.getBoundingClientRect();
  const px = event.clientX - rect.left;
  const total = activeReport.history.length + activeReport.forecasts.length;
  const index = Math.round((px - 57) / (canvas.clientWidth - 57 - 14) * (total - 1));
  if (px < 57 || px > canvas.clientWidth - 14) {
    if (hoverIndex !== -1) { hoverIndex = -1; el('chartTooltip').hidden = true; drawChart(); }
    return;
  }
  const target = chartPoints.findIndex((point) => point.index === Math.min(total - 1, Math.max(0, index)));
  if (target !== hoverIndex) { hoverIndex = target; drawChart(); }
}

el('windowInput').addEventListener('input', () => {
  el('windowValue').textContent = `${el('windowInput').value} observations`;
});
el('horizonInput').addEventListener('input', () => {
  el('horizonValue').textContent = `${el('horizonInput').value} steps`;
});
el('analyzeBtn').addEventListener('click', analyze);
el('downloadBtn').addEventListener('click', downloadForecast);
el('sampleBtn').addEventListener('click', loadDemo);
el('fileInput').addEventListener('change', (event) => handleFile(event.target.files[0]));
el('dropZone').addEventListener('click', () => el('fileInput').click());
el('dropZone').addEventListener('keydown', (event) => {
  if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); el('fileInput').click(); }
});
['dragenter', 'dragover'].forEach((type) => el('dropZone').addEventListener(type, (event) => {
  event.preventDefault(); el('dropZone').classList.add('dragging');
}));
['dragleave', 'drop'].forEach((type) => el('dropZone').addEventListener(type, (event) => {
  event.preventDefault(); el('dropZone').classList.remove('dragging');
}));
el('dropZone').addEventListener('drop', (event) => handleFile(event.dataTransfer.files[0]));
el('priceChart').addEventListener('pointermove', onChartMove);
el('priceChart').addEventListener('pointerleave', () => {
  hoverIndex = -1;
  el('chartTooltip').hidden = true;
  drawChart();
});
window.addEventListener('resize', drawChart);
loadDemo();
