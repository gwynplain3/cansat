<script>
  import { onMount, onDestroy } from "svelte";
  import { tweened } from "svelte/motion";
  import { cubicOut } from "svelte/easing";
  import { io } from "socket.io-client";
  import {
    Chart,
    LineController,
    LineElement,
    PointElement,
    LinearScale,
    CategoryScale,
    Filler,
    Tooltip,
  } from "chart.js";

  Chart.register(
    LineController,
    LineElement,
    PointElement,
    LinearScale,
    CategoryScale,
    Filler,
    Tooltip,
  );

  // ─── Configuration ──────────────────────────────────────────────────────────
  const MAX_POINTS = 30;
  const SERVER_URL = "http://localhost:3000";

  // ─── Smooth Motion (Tweening) ────────────────────────────────────────────────
  // These make the big numbers "glide" smoothly instead of jumping
  const smoothAlt = tweened(0, { duration: 600, easing: cubicOut });
  const smoothSpeed = tweened(0, { duration: 600, easing: cubicOut });
  const smoothTemp = tweened(25, { duration: 1000, easing: cubicOut });
  const smoothRSSI = tweened(-120, { duration: 400, easing: cubicOut });

  // ─── State ───────────────────────────────────────────────────────────────────
  let connected = false;
  let lastPacketId = 0;
  let packetLossCount = 0;

  let data = {
    time: 0,
    alt: 0,
    press: 0,
    temp: 0,
    hum: 0,
    lux: 0,
    acc: { x: 0, y: 0, z: 0 },
    gps: { lat: 0, lng: 0 },
    speed: 0,
    dust: 0,
    id: 0,
    rssi: -120,
  };

  let altHistory = [];
  let timeLabels = [];
  let chartCanvas;
  let chart = null;
  let socket = null;

  // ─── Reactive Calculations (UI Sync) ────────────────────────────────────────
  $: rssiPercent = Math.max(0, Math.min(100, (($smoothRSSI + 120) / 80) * 100));
  $: rssiColor =
    data.rssi > -70 ? "#34C759" : data.rssi > -90 ? "#FF9500" : "#FF3B30";

  $: accXPercent = Math.min(100, (Math.abs(data.acc.x) / 20) * 100);
  $: accYPercent = Math.min(100, (Math.abs(data.acc.y) / 20) * 100);
  $: accZPercent = Math.min(100, (Math.abs(data.acc.z) / 20) * 100);

  // ─── Data Processing ────────────────────────────────────────────────────────
  function processPacket(packet) {
    // Detect packet loss for judges
    if (lastPacketId !== 0 && packet.id > lastPacketId + 1) {
      packetLossCount += packet.id - lastPacketId - 1;
    }
    lastPacketId = packet.id;

    data = packet;

    // Trigger smooth number glides
    smoothAlt.set(parseFloat(packet.alt) || 0);
    smoothSpeed.set(parseFloat(packet.speed) || 0);
    smoothTemp.set(parseFloat(packet.temp) || 0);
    smoothRSSI.set(parseInt(packet.rssi) || -120);

    // Update Chart
    const timeLabel = `T+${Math.floor(packet.time / 1000)}s`;
    altHistory = [
      ...altHistory.slice(-(MAX_POINTS - 1)),
      parseFloat(packet.alt),
    ];
    timeLabels = [...timeLabels.slice(-(MAX_POINTS - 1)), timeLabel];

    updateChart();
  }

  // ─── Chart Setup ─────────────────────────────────────────────────────────────
  function initChart() {
    if (!chartCanvas) return;
    const ctx = chartCanvas.getContext("2d");
    const gradient = ctx.createLinearGradient(0, 0, 0, 140);
    gradient.addColorStop(0, "rgba(0, 122, 255, 0.18)");
    gradient.addColorStop(1, "rgba(0, 122, 255, 0)");

    chart = new Chart(ctx, {
      type: "line",
      data: {
        labels: timeLabels,
        datasets: [
          {
            data: altHistory,
            borderColor: "#007AFF",
            borderWidth: 2,
            pointRadius: 0,
            fill: true,
            backgroundColor: gradient,
            tension: 0.4,
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: { legend: { display: false }, tooltip: { enabled: false } },
        scales: {
          x: { display: false },
          y: {
            grid: { display: false },
            border: { display: false },
            ticks: { color: "#AEAEB2", font: { size: 10 }, maxTicksLimit: 3 },
          },
        },
      },
    });
  }

  function updateChart() {
    if (!chart) return;
    chart.data.labels = timeLabels;
    chart.data.datasets[0].data = altHistory;
    chart.update("none");
  }

  onMount(() => {
    initChart();
    socket = io(SERVER_URL, { transports: ["websocket"] });
    socket.on("connect", () => (connected = true));
    socket.on("disconnect", () => (connected = false));
    socket.on("flight-data", processPacket);
  });

  onDestroy(() => {
    socket?.disconnect();
    chart?.destroy();
  });
</script>

<div class="shell">
  <!-- HEADER -->
  <header class="header">
    <div class="header-left">
      <div class="logo-icon">
        <svg
          width="20"
          height="20"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="2.2"
        >
          <path d="M12 2L2 7l10 5 10-5-10-5z" /><path
            d="M2 17l10 5 10-5"
          /><path d="M2 12l10 5 10-5" />
        </svg>
      </div>
      <div class="logo-text">
        <span class="logo-title">Team RWB</span>
        <span class="logo-sub">CanSat Mission Control</span>
      </div>
    </div>
    <div class="header-right">
      {#if packetLossCount > 0}
        <span class="loss-pill">LOSS: {packetLossCount}</span>
      {/if}
      <span class="packet-id">PKT #{data.id}</span>
      <div class="status-pill" class:live={connected}>
        <span class="status-dot"></span>
        <span class="status-label">{connected ? "LIVE" : "DISCONNECTED"}</span>
      </div>
    </div>
  </header>

  <!-- MAIN GRID -->
  <main class="grid">
    <!-- HERO CARD: ALTITUDE -->
    <section class="card hero-card">
      <div class="card-label">ALTITUDE</div>
      <div class="hero-value">
        {$smoothAlt.toFixed(1)} <span class="hero-unit">m</span>
      </div>
      <div class="chart-wrapper">
        <canvas bind:this={chartCanvas}></canvas>
      </div>
    </section>

    <!-- SECONDARY ROW -->
    <div class="secondary-row">
      <section class="card stat-card">
        <div class="card-label">SPEED</div>
        <div class="stat-value">{$smoothSpeed.toFixed(1)}</div>
        <div class="stat-unit">km/h</div>
      </section>

      <section class="card stat-card">
        <div class="card-label">TEMPERATURE</div>
        <div class="stat-value">{$smoothTemp.toFixed(1)}</div>
        <div class="stat-unit">°C</div>
      </section>

      <section class="card stat-card">
        <div class="card-label">SIGNAL (RSSI)</div>
        <div class="stat-value" style="color: {rssiColor}">
          {$smoothRSSI.toFixed(0)}
        </div>
        <div class="stat-unit">dBm</div>
        <div class="rssi-bar-bg">
          <div
            class="rssi-bar-fill"
            style="width: {rssiPercent}%; background: {rssiColor};"
          ></div>
        </div>
      </section>
    </div>

    <!-- BOTTOM ROW -->
    <div class="bottom-row">
      <!-- GPS -->
      <section class="card gps-card">
        <div class="card-label">GPS COORDINATES</div>
        <div class="gps-coords">
          <div class="gps-item">
            <span class="gps-axis">LAT</span>
            <span class="gps-val">{data.gps.lat}°</span>
          </div>
          <div class="gps-divider"></div>
          <div class="gps-item">
            <span class="gps-axis">LNG</span>
            <span class="gps-val">{data.gps.lng}°</span>
          </div>
        </div>
      </section>

      <!-- ACCELEROMETER -->
      <section class="card acc-card">
        <div class="card-label">ACCELEROMETER (G)</div>
        <div class="acc-grid">
          <div class="acc-axis">
            <span class="acc-label x">X</span>
            <div class="acc-bar-wrap">
              <div class="acc-bar-fill x" style="width: {accXPercent}%"></div>
            </div>
            <span class="acc-num">{data.acc.x}</span>
          </div>
          <div class="acc-axis">
            <span class="acc-label y">Y</span>
            <div class="acc-bar-wrap">
              <div class="acc-bar-fill y" style="width: {accYPercent}%"></div>
            </div>
            <span class="acc-num">{data.acc.y}</span>
          </div>
          <div class="acc-axis">
            <span class="acc-label z">Z</span>
            <div class="acc-bar-wrap">
              <div class="acc-bar-fill z" style="width: {accZPercent}%"></div>
            </div>
            <span class="acc-num">{data.acc.z}</span>
          </div>
        </div>
      </section>

      <!-- ENVIRONMENT -->
      <section class="card env-card">
        <div class="card-label">ENVIRONMENT</div>
        <div class="env-grid">
          <div class="env-item">
            <span class="env-name">Pressure</span>
            <span class="env-val">{data.press} <small>hPa</small></span>
          </div>
          <div class="env-item">
            <span class="env-name">Humidity</span>
            <span class="env-val">{data.hum} <small>%</small></span>
          </div>
          <div class="env-item">
            <span class="env-name">Light</span>
            <span class="env-val">{data.lux} <small>lx</small></span>
          </div>
          <div class="env-item">
            <span class="env-name">Dust</span>
            <span class="env-val">{data.dust} <small>µg</small></span>
          </div>
        </div>
      </section>
    </div>
  </main>
</div>

<style>
  :global(body) {
    margin: 0;
    font-family:
      "Inter",
      -apple-system,
      sans-serif;
  }
  .shell {
    min-height: 100vh;
    background: #f5f5f7;
    color: #1d1d1f;
  }

  /* HEADER */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 28px;
    background: rgba(255, 255, 255, 0.8);
    backdrop-filter: blur(20px);
    border-bottom: 1px solid rgba(0, 0, 0, 0.05);
    position: sticky;
    top: 0;
    z-index: 100;
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .logo-icon {
    width: 36px;
    height: 36px;
    background: #007aff;
    border-radius: 10px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: white;
  }
  .logo-title {
    font-size: 14px;
    font-weight: 700;
    display: block;
  }
  .logo-sub {
    font-size: 11px;
    color: #86868b;
  }
  .header-right {
    display: flex;
    align-items: center;
    gap: 15px;
  }
  .packet-id {
    font-size: 12px;
    color: #aeaeb2;
    font-weight: 600;
  }
  .loss-pill {
    font-size: 10px;
    font-weight: 800;
    color: white;
    background: #ff3b30;
    padding: 4px 8px;
    border-radius: 6px;
  }

  .status-pill {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 6px 12px;
    border-radius: 20px;
    background: rgba(0, 0, 0, 0.05);
    border: 1px solid rgba(0, 0, 0, 0.05);
  }
  .status-pill.live {
    background: rgba(52, 199, 89, 0.1);
    border-color: rgba(52, 199, 89, 0.2);
  }
  .status-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: #aeaeb2;
  }
  .status-pill.live .status-dot {
    background: #34c759;
    box-shadow: 0 0 8px #34c759;
  }
  .status-label {
    font-size: 11px;
    font-weight: 700;
    color: #86868b;
  }
  .status-pill.live .status-label {
    color: #34c759;
  }

  /* GRID */
  .grid {
    padding: 20px 28px;
    display: flex;
    flex-direction: column;
    gap: 20px;
  }
  .card {
    background: white;
    border-radius: 22px;
    padding: 24px;
    box-shadow: 0 4px 20px rgba(0, 0, 0, 0.03);
  }
  .card-label {
    font-size: 11px;
    font-weight: 700;
    color: #aeaeb2;
    letter-spacing: 0.05em;
    margin-bottom: 15px;
  }

  /* HERO */
  .hero-card {
    background: #fff;
    position: relative;
    overflow: hidden;
  }
  .hero-value {
    font-size: 72px;
    font-weight: 200;
    letter-spacing: -2px;
  }
  .hero-unit {
    font-size: 24px;
    color: #aeaeb2;
    font-weight: 400;
  }
  .chart-wrapper {
    height: 120px;
    margin-top: 10px;
  }

  /* SECONDARY */
  .secondary-row {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 20px;
  }
  .stat-value {
    font-size: 38px;
    font-weight: 300;
  }
  .stat-unit {
    font-size: 13px;
    color: #aeaeb2;
    font-weight: 600;
    margin-top: 4px;
  }
  .rssi-bar-bg {
    height: 4px;
    background: #f2f2f7;
    border-radius: 2px;
    margin-top: 15px;
  }
  .rssi-bar-fill {
    height: 100%;
    border-radius: 2px;
    transition: width 0.5s ease;
  }

  /* BOTTOM */
  .bottom-row {
    display: grid;
    grid-template-columns: 1.2fr 1fr 1.2fr;
    gap: 20px;
  }
  .gps-coords {
    display: flex;
    background: #f5f5f7;
    border-radius: 14px;
  }
  .gps-item {
    padding: 15px;
    flex: 1;
  }
  .gps-axis {
    font-size: 10px;
    font-weight: 700;
    color: #aeaeb2;
    display: block;
  }
  .gps-val {
    font-size: 16px;
    font-weight: 600;
  }
  .gps-divider {
    width: 1px;
    background: rgba(0, 0, 0, 0.05);
  }

  .acc-grid {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .acc-axis {
    display: grid;
    grid-template-columns: 15px 1fr 40px;
    align-items: center;
    gap: 10px;
  }
  .acc-label {
    font-size: 11px;
    font-weight: 800;
  }
  .acc-bar-wrap {
    height: 6px;
    background: #f2f2f7;
    border-radius: 3px;
    overflow: hidden;
  }
  .acc-bar-fill {
    height: 100%;
    border-radius: 3px;
  }
  .acc-bar-fill.x {
    background: #007aff;
  }
  .acc-bar-fill.y {
    background: #34c759;
  }
  .acc-bar-fill.z {
    background: #ff9500;
  }
  .acc-num {
    font-size: 11px;
    color: #86868b;
    text-align: right;
  }

  .env-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .env-item {
    background: #f5f5f7;
    padding: 12px;
    border-radius: 12px;
  }
  .env-name {
    font-size: 10px;
    font-weight: 700;
    color: #aeaeb2;
    text-transform: uppercase;
  }
  .env-val {
    font-size: 16px;
    font-weight: 600;
    display: block;
  }
  .env-val small {
    font-size: 10px;
    color: #86868b;
  }

  @media (max-width: 800px) {
    .secondary-row,
    .bottom-row {
      grid-template-columns: 1fr;
    }
  }
</style>
