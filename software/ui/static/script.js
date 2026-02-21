const nodesTbody = document.getElementById("nodes-tbody");
const packetsTbody = document.getElementById("packets-tbody");
const nodesMeta = document.getElementById("nodes-meta");
const packetsMeta = document.getElementById("packets-meta");
const connPill = document.getElementById("conn-pill");

const filterInput = document.getElementById("filter");
const dirSelect = document.getElementById("dir");
const refreshBtn = document.getElementById("refresh-btn");

let nodesCache = [];
let packetsCache = [];

function escapeHtml(s){
  return String(s)
    .replaceAll("&","&amp;")
    .replaceAll("<","&lt;")
    .replaceAll(">","&gt;")
    .replaceAll('"',"&quot;")
    .replaceAll("'","&#039;");
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

function renderNodes(){
  if (!nodesCache.length){
    nodesTbody.innerHTML = `<tr><td colspan="5" class="muted">No nodes yet.</td></tr>`;
    nodesMeta.textContent = "0 nodes";
    return;
  }
  nodesMeta.textContent = `${nodesCache.length} nodes`;
  nodesTbody.innerHTML = nodesCache.map(n => `
    <tr>
      <td><strong>${escapeHtml(n.node_id ?? n.id ?? "—")}</strong></td>
      <td>${qualityBar(n.quality)}</td>
      <td class="muted">${escapeHtml(n.rssi ?? "—")}</td>
      <td class="muted">${escapeHtml(n.last_seen ?? (n.last_seen_ms ? `${Math.round(n.last_seen_ms/1000)}s ago` : "—"))}</td>
      <td>${statusBadge(n.status)}</td>
    </tr>
  `).join("");
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

async function refresh(){
  try{
    connPill.textContent = "Backend: online";
    nodesCache = await fetchJson("/api/nodes");
    packetsCache = await fetchJson("/api/packets?limit=200");
    renderNodes();
    renderPackets();
  }catch(e){
    connPill.textContent = "Backend: offline";
    nodesTbody.innerHTML = `<tr><td colspan="5" class="muted">Backend not responding.</td></tr>`;
    packetsTbody.innerHTML = `<tr><td colspan="7" class="muted">Backend not responding.</td></tr>`;
    nodesMeta.textContent = "—";
    packetsMeta.textContent = "—";
  }
}

filterInput.addEventListener("input", renderPackets);
dirSelect.addEventListener("change", renderPackets);
refreshBtn.addEventListener("click", refresh);

refresh();
setInterval(refresh, 2000);