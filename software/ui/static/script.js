const nodesTbody = document.getElementById("nodes-tbody");
const packetsTbody = document.getElementById("packets-tbody");
const nodesMeta = document.getElementById("nodes-meta");
const packetsMeta = document.getElementById("packets-meta");
const connPill = document.getElementById("conn-pill");

const filterInput = document.getElementById("filter");
const dirSelect = document.getElementById("dir");
const refreshBtn = document.getElementById("refresh-btn");

const portSelect = document.getElementById("port-select");
const portRefreshBtn = document.getElementById("port-refresh-btn");
const serialConnectBtn = document.getElementById("serial-connect-btn");
const serialStatus = document.getElementById("serial-status");

const radarCanvas = document.getElementById("radar");
const radarCtx = radarCanvas ? radarCanvas.getContext("2d") : null;
const radarModeEl = document.getElementById("radar-mode");

const STALE_MS = 30_000;
const OFFLINE_MS = 120_000;

let nodesMap = {};
let packetsCache = [];
let serialConnected = false;
let localNode = { id: null, lat: null, lon: null };

function fmtHex(id) {
  if (!id) return "—";
  const s = String(id).replace(/^0x/i, "").toUpperCase();
  return "0x" + s;
}

function escapeHtml(s){
  return String(s)
    .replaceAll("&","&amp;")
    .replaceAll("<","&lt;")
    .replaceAll(">","&gt;")
    .replaceAll('"',"&quot;")
    .replaceAll("'","&#039;");
}

function stableAngleFromId(id){
  const s = String(id || "");
  let h = 0;
  for (let i = 0; i < s.length; i++) h = (h * 31 + s.charCodeAt(i)) >>> 0;
  return (h % 360) * Math.PI / 180;
}

function drawRadar(){
  if (!radarCtx || !radarCanvas) return;

  const W = radarCanvas.width;
  const H = radarCanvas.height;
  const cx = W / 2, cy = H / 2;
  const R = Math.min(W, H) * 0.42;

  radarCtx.clearRect(0, 0, W, H);

  radarCtx.strokeStyle = "rgba(255,255,255,.12)";
  radarCtx.lineWidth = 1;
  for (let i = 1; i <= 4; i++){
    radarCtx.beginPath();
    radarCtx.arc(cx, cy, (R * i) / 4, 0, Math.PI * 2);
    radarCtx.stroke();
  }
  radarCtx.beginPath(); radarCtx.moveTo(cx - R, cy); radarCtx.lineTo(cx + R, cy); radarCtx.stroke();
  radarCtx.beginPath(); radarCtx.moveTo(cx, cy - R); radarCtx.lineTo(cx, cy + R); radarCtx.stroke();

  radarCtx.fillStyle = "rgba(0,255,170,1)";
  radarCtx.beginPath();
  radarCtx.arc(cx, cy, 5, 0, Math.PI * 2);
  radarCtx.fill();

  const nodes = Object.values(nodesMap).filter(n => n && n.id);
  const hasLocalGps = localNode.lat != null && localNode.lon != null;

  if (radarModeEl){
    radarModeEl.textContent = hasLocalGps ? "Mode: GPS" : "Mode: estimated";
  }

  if (hasLocalGps){
    const lat0 = Number(localNode.lat);
    const lon0 = Number(localNode.lon);

    const gpsNodes = nodes.filter(n => n.lat != null && n.lon != null && !n.revoked);
    if (!gpsNodes.length) return;

    const pts = gpsNodes.map(n => {
      const { x, y } = gpsToMeters(Number(n.lat), Number(n.lon), lat0, lon0);
      return { n, x, y, d: Math.hypot(x, y) };
    });
    const maxD = Math.max(1, ...pts.map(p => p.d));
    const scale = R / maxD;

    for (const p of pts){
      const px = cx + p.x * scale;
      const py = cy - p.y * scale;

      const status = getNodeStatus(p.n);
      const color = status === "online" ? "rgba(0,255,170,1)"
                  : status === "stale"  ? "rgba(255,196,0,1)"
                  : "rgba(255,80,80,1)";

      radarCtx.fillStyle = color;
      radarCtx.beginPath();
      radarCtx.arc(px, py, 4, 0, Math.PI * 2);
      radarCtx.fill();

      radarCtx.fillStyle = "rgba(232,236,255,.85)";
      radarCtx.font = "12px ui-sans-serif, system-ui";
      radarCtx.fillText(fmtHex(p.n.id), px + 7, py - 7);
    }
    return;
  }

  for (const n of nodes){
    const status = getNodeStatus(n);
    const angle = stableAngleFromId(n.id);
    const dist = R * estimatedDistance01(n);

    const px = cx + Math.cos(angle) * dist;
    const py = cy + Math.sin(angle) * dist;

    const color = status === "online" ? "rgba(0,255,170,1)"
                : status === "stale"  ? "rgba(255,196,0,1)"
                : "rgba(255,80,80,1)";

    radarCtx.fillStyle = color;
    radarCtx.beginPath();
    radarCtx.arc(px, py, 4, 0, Math.PI * 2);
    radarCtx.fill();

    radarCtx.fillStyle = "rgba(232,236,255,.85)";
    radarCtx.font = "12px ui-sans-serif, system-ui";
    radarCtx.fillText(fmtHex(n.id), px + 7, py - 7);
  }
}

function estimatedDistance01(node){

  const age = Date.now() - (node.last_seen || 0);
  const t = Math.max(0, Math.min(1, age / OFFLINE_MS));
  return 0.2 + 0.8 * t; 
}

function gpsToMeters(lat, lon, lat0, lon0){
  const x = (lon - lon0) * 111320 * Math.cos(lat0 * Math.PI / 180); 
  const y = (lat - lat0) * 110540; 
  return { x, y };
}

function statusBadge(status){
  const s = (status || "offline").toLowerCase();
  const cls = ["online","stale","offline"].includes(s) ? s : "offline";
  const dot = cls === "online" ? "rgba(0,255,170,1)"
            : cls === "stale"  ? "rgba(255,196,0,1)"
            : "rgba(255,80,80,1)";
  return `
    <span class="badge ${cls}">
      <span class="dot" style="background:${dot}"></span>
      ${escapeHtml(cls.toUpperCase())}
    </span>
  `;
}

function qualityBar(q){
  const val = Math.max(0, Math.min(100, Number(q) || 0));
  const w = val;
  return `
    <div style="display:flex;align-items:center;gap:10px;min-width:160px;">
      <div style="flex:1;height:8px;border:1px solid rgba(255,255,255,.12);border-radius:999px;overflow:hidden;background:rgba(255,255,255,.03);">
        <div style="height:100%;width:${w}%;background:rgba(86,115,255,1);"></div>
      </div>
      <span class="muted" style="min-width:34px;text-align:right;">${val}%</span>
    </div>
  `;
}

function fmtTime(ts){
  if (!ts) return "—";
  const t = Number(ts);
  const ms = t > 1e12 ? t : t * 1000;
  const d = new Date(ms);
  return d.toLocaleTimeString([], {hour:"2-digit", minute:"2-digit", second:"2-digit"});
}

function getNodeStatus(node) {
  const age = Date.now() - node.last_seen;
  if (node.revoked) return "offline";
  if (age > OFFLINE_MS) return "offline";
  if (age > STALE_MS) return "stale";
  return "online";
}

function fmtAge(ms) {
  const sec = Math.round((Date.now() - ms) / 1000);
  if (sec < 60) return `${sec}s ago`;
  return `${Math.floor(sec / 60)}m ${sec % 60}s ago`;
}

function updateNode(data) {
  const id = data.id;
  if (!id) return;

  const existing = nodesMap[id] || { id, last_seen: 0, revoked: false };

  if (data.type === "revoke") {
    existing.revoked = true;
  } else if (data.type === "reinstate") {
    existing.revoked = false;
  }

  existing.last_seen = Date.now();

  if (data.type === "position") {
    existing.lat = data.lat;
    existing.lon = data.lon;
  }

  nodesMap[id] = existing;
  renderNodes();
}

function renderNodes(){
  const nodes = Object.values(nodesMap);
  if (!nodes.length){
    nodesTbody.innerHTML = `<tr><td colspan="4" class="muted">No nodes yet.</td></tr>`;
    nodesMeta.textContent = "0 nodes";
    return;
  }
  nodes.sort((a, b) => b.last_seen - a.last_seen);
  nodesMeta.textContent = `${nodes.length} nodes`;
  nodesTbody.innerHTML = nodes.map(n => `
    <tr>
      <td><strong>${escapeHtml(fmtHex(n.id))}</strong></td>
      <td class="muted">${escapeHtml(n.lat && n.lon ? `${n.lat}, ${n.lon}` : "—")}</td>
      <td class="muted">${fmtAge(n.last_seen)}</td>
      <td>${statusBadge(getNodeStatus(n))}</td>
    </tr>
  `).join("");
  drawRadar();
}

function applyPacketFilters(list){
  const term = (filterInput.value || "").trim().toLowerCase();
  const dir = (dirSelect.value || "").trim().toLowerCase();

  return list.filter(p => {
    if (dir && String(p.dir).toLowerCase() !== dir) return false;
    if (!term) return true;
    const hay = `${p.from} ${p.to} ${p.type} ${p.result} ${p.dir}`.toLowerCase();
    return hay.includes(term);
  });
}

function renderPackets(){
  const filtered = applyPacketFilters(packetsCache);

  packetsMeta.textContent = `${filtered.length}/${packetsCache.length} shown`;
  if (!filtered.length){
    packetsTbody.innerHTML = `<tr><td colspan="7" class="muted">No packets match your filters.</td></tr>`;
    return;
  }

  packetsTbody.innerHTML = filtered.map(p => `
    <tr>
      <td class="muted">${escapeHtml(fmtTime(p.ts))}</td>
      <td><strong>${escapeHtml((p.dir || "—").toUpperCase())}</strong></td>
      <td>${escapeHtml(p.from ?? "—")}</td>
      <td>${escapeHtml(p.to ?? "—")}</td>
      <td>${escapeHtml(p.type ?? "—")}</td>
      <td class="muted">${escapeHtml(p.bytes ?? "—")}</td>
      <td class="muted">${escapeHtml(p.result ?? "—")}</td>
    </tr>
  `).join("");
}

async function fetchJson(url){
  const res = await fetch(url, { cache: "no-store" });
  if (!res.ok) throw new Error(`${url} → ${res.status}`);
  return await res.json();
}

function refresh(){
  renderNodes();
  renderPackets();
}

let serialSource = null;

function startSerialStream() {
  if (serialSource) serialSource.close();

  serialSource = new EventSource("/api/serial/stream");

  serialSource.onmessage = (event) => {
    const data = JSON.parse(event.data);

    if (data.type === "identity") {
      localNode.id = data.id;
      updateLocalInfo();
      return;
    }
    if (data.type === "mypos") {
      localNode.lat = data.lat;
      localNode.lon = data.lon;
      updateLocalInfo();
      return;
    }

    updateNode(data);
    packetsCache.unshift({
      ts: data.timestamp,
      dir: "IN",
      from: fmtHex(data.id),
      to: localNode.id ? fmtHex(localNode.id) : "LOCAL",
      type: data.type ?? "—",
      bytes: "—",
      result: "OK",
    });
    if (packetsCache.length > 500) packetsCache.length = 500;
    renderPackets();
  };

  serialSource.onerror = () => {
    connPill.textContent = "Serial: disconnected";
  };

  serialSource.onopen = () => {
    connPill.textContent = "Serial: streaming";
  };
}

function updateLocalInfo() {
  const idStr = localNode.id ? `Node ${fmtHex(localNode.id)}` : "";
  const posStr = localNode.lat && localNode.lon ? `${localNode.lat}, ${localNode.lon}` : "";
  drawRadar();
  const localNodeId = document.getElementById("local-node-id");
  const localNodePos = document.getElementById("local-node-pos");
  if (localNodeId) localNodeId.textContent = idStr || "Querying…";
  if (localNodePos) localNodePos.textContent = posStr || "Unknown";
  if (localNode.id) {
    serialStatus.textContent = `Connected — Node ${fmtHex(localNode.id)}`;
  }
}

function stopSerialStream() {
  if (serialSource) {
    serialSource.close();
    serialSource = null;
    connPill.textContent = "Serial: closed";
  }
}

async function loadPorts() {
  portSelect.disabled = true;
  try {
    const ports = await fetchJson("/api/ports");
    portSelect.innerHTML = '<option value="">Select a port…</option>';
    ports.forEach(p => {
      const opt = document.createElement("option");
      opt.value = p.device;
      opt.textContent = `${p.device} — ${p.description}`;
      portSelect.appendChild(opt);
    });
  } catch {
    portSelect.innerHTML = '<option value="">Failed to load ports</option>';
  }
  portSelect.disabled = false;
}

async function connectSerial() {
  const port = portSelect.value;
  if (!port) return;

  serialConnectBtn.disabled = true;
  try {
    const res = await fetch("/api/serial/connect", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ port }),
    });
    if (!res.ok) throw new Error("connect failed");
    serialConnected = true;
    serialConnectBtn.textContent = "Disconnect";
    serialConnectBtn.classList.add("connected");
    serialStatus.textContent = `Connected to ${port}`;
    portSelect.disabled = true;
    portRefreshBtn.disabled = true;
    startSerialStream();
    fetch("/api/serial/send", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command: "id" }),
    });
  } catch {
    serialStatus.textContent = "Connection failed";
  }
  serialConnectBtn.disabled = false;
}

async function disconnectSerial() {
  serialConnectBtn.disabled = true;
  stopSerialStream();
  try {
    await fetch("/api/serial/disconnect", { method: "POST" });
  } catch { }
  serialConnected = false;
  localNode = { id: null, lat: null, lon: null };
  serialConnectBtn.textContent = "Connect";
  serialConnectBtn.classList.remove("connected");
  serialStatus.textContent = "Disconnected";
  const localNodeId = document.getElementById("local-node-id");
  const localNodePos = document.getElementById("local-node-pos");
  if (localNodeId) localNodeId.textContent = "—";
  if (localNodePos) localNodePos.textContent = "—";
  portSelect.disabled = false;
  portRefreshBtn.disabled = false;
  serialConnectBtn.disabled = false;
}

const cmdInput = document.getElementById("cmd-input");
const cmdSendBtn = document.getElementById("cmd-send-btn");
const cmdStatus = document.getElementById("cmd-status");

async function sendCommand() {
  const command = cmdInput.value.trim();
  if (!command) return;
  if (!serialConnected) {
    cmdStatus.textContent = "Not connected";
    return;
  }

  cmdSendBtn.disabled = true;
  try {
    const res = await fetch("/api/serial/send", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command }),
    });
    if (!res.ok) throw new Error("send failed");
    const parts = command.split(" ");
    const cmd = parts[0];
    let to = "BROADCAST";
    if (cmd === "msg" && parts[1]) to = fmtHex(parts[1]);
    else if (cmd === "revoke" && parts[1]) to = fmtHex(parts[1]);
    else if (cmd === "reinstate" && parts[1]) to = fmtHex(parts[1]);
    packetsCache.unshift({
      ts: Date.now() / 1000,
      dir: "OUT",
      from: localNode.id ? fmtHex(localNode.id) : "LOCAL",
      to,
      type: cmd,
      bytes: "—",
      result: "OK",
    });
    if (packetsCache.length > 500) packetsCache.length = 500;
    renderPackets();
    cmdStatus.textContent = `Sent: ${command}`;
    cmdInput.value = "";
  } catch {
    cmdStatus.textContent = "Failed to send";
  }
  cmdSendBtn.disabled = false;
}

cmdSendBtn.addEventListener("click", sendCommand);
cmdInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") sendCommand();
});

document.querySelectorAll(".cmd-shortcut").forEach(btn => {
  btn.addEventListener("click", () => {
    const prefix = btn.dataset.prefix;
    cmdInput.value = prefix;
    cmdInput.focus();
    if (prefix === "pos") sendCommand();
  });
});

serialConnectBtn.addEventListener("click", () => {
  if (serialConnected) disconnectSerial();
  else connectSerial();
});

portRefreshBtn.addEventListener("click", loadPorts);

filterInput.addEventListener("input", renderPackets);
dirSelect.addEventListener("change", renderPackets);
refreshBtn.addEventListener("click", refresh);

loadPorts();
renderNodes();
renderPackets();
setInterval(refresh, 5000);