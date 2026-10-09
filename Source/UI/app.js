/* GSAliveAirport UI — WebView bridge + interactions */
"use strict";

const bridge = {
  send(command) {
    const webview = window.chrome && window.chrome.webview;
    if (webview) {
      webview.postMessage(command);
      return true;
    }
    console.error("WebView bridge unavailable, command ignored:", command);
    return false;
  },
};

const logElement = document.getElementById("log");
const logPlaceholder = document.getElementById("log-placeholder");
const logCount = document.getElementById("log-count");
const statusPill = document.getElementById("status-pill");
const statusText = document.getElementById("status-text");
const versionChip = document.getElementById("version-chip");

const MAX_LOG_LINES = 2000;
let logLineCount = 0;
let sawBanner = false;

/* ---- Persistent UI state (the WebView2 profile now survives across runs) ----
   Version guard: when the UI version changes, drop state saved by an older
   build so stale keys never leak into the new one. */
(function initStoredState() {
  try {
    const version = versionChip ? versionChip.textContent.trim() : "";
    if (!version) return;
    const key = "gsalive.version";
    if (localStorage.getItem(key) !== version) {
      localStorage.clear();
      localStorage.setItem(key, version);
    }
  } catch (e) {
    console.error("localStorage unavailable, running stateless:", e);
  }
})();

function removePlaceholder() {
  if (logPlaceholder && logPlaceholder.parentNode) logPlaceholder.remove();
}

function updateLogCount() {
  logCount.textContent = logLineCount > 0 ? String(logLineCount) : "";
}

function appendLogLine(text) {
  removePlaceholder();
  const line = document.createElement("div");
  line.className = "log-line";
  if (/[Ee]rror|fail|exception|denied|invalid/.test(text)) line.classList.add("error");
  if (/Connected to MSFS/.test(text)) line.classList.add("ok");
  line.textContent = text;

  const nearBottom = logElement.scrollHeight - logElement.scrollTop - logElement.clientHeight < 80;
  logElement.appendChild(line);
  while (logElement.childElementCount > MAX_LOG_LINES) {
    logElement.firstElementChild.remove();
    if (logLineCount > 0) logLineCount -= 1;
  }
  logLineCount += 1;
  if (nearBottom) logElement.scrollTop = logElement.scrollHeight;
  updateLogCount();
  updateStatus(text);
}

function updateStatus(text) {
  if (!sawBanner && /GSAliveAirport - Alive Airport/.test(text)) {
    sawBanner = true;
    statusPill.classList.remove("disconnected");
    statusText.textContent = "Waiting for MSFS 2024…";
    const versionMatch = text.match(/Version:\s*([0-9][0-9.]*[0-9])/);
    if (versionMatch) versionChip.textContent = versionMatch[1];
    return;
  }
  if (/Connected to MSFS/.test(text)) {
    statusPill.classList.remove("disconnected");
    statusPill.classList.add("connected");
    statusText.textContent = "Connected to MSFS 2024";
  } else if (/closed the SimConnect connection|reconnecting|dispatch failed/.test(text)) {
    statusPill.classList.remove("connected");
    statusPill.classList.add("disconnected");
    statusText.textContent = "Sim connection lost";
  }
}

/* ---- Command buttons ---- */
document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", () => bridge.send(button.dataset.command));
});

/* ---- Log toggle ---- */
let loggingEnabled = true;
const logButton = document.getElementById("btn-log");
logButton.addEventListener("click", () => {
  if (!bridge.send("log")) return;
  loggingEnabled = !loggingEnabled;
  logButton.classList.toggle("on", loggingEnabled);
  logButton.setAttribute("aria-pressed", String(loggingEnabled));
});

/* ---- Debug loggers ---- */
const debugInput = document.getElementById("debug-input");
const debugButton = document.getElementById("btn-debug");
debugButton.addEventListener("click", () => {
  const value = debugInput.value.trim();
  bridge.send(value ? "debug " + value : "debug");
});
debugInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") debugButton.click();
});

/* ---- Clear log ---- */
document.getElementById("btn-clear").addEventListener("click", () => {
  logElement.querySelectorAll(".log-line").forEach((element) => element.remove());
  logLineCount = 0;
  if (logPlaceholder && !logPlaceholder.parentNode) logElement.appendChild(logPlaceholder);
  updateLogCount();
});

/* ---- Left rail navigation ---- */
const navButtons = Array.from(document.querySelectorAll(".sidebar .nav-btn"));
const pages = Array.from(document.querySelectorAll(".content .page"));
navButtons.forEach((button) => {
  button.addEventListener("click", () => {
    const name = button.dataset.page;
    navButtons.forEach((b) => b.classList.toggle("active", b === button));
    pages.forEach((page) => page.classList.toggle("active", page.id === "page-" + name));
  });
});

/* ---- User aircraft pane (fixed above the scrolling list, always first) ---- */
const userChip = document.getElementById("user-chip");
const userFields = {
  id: document.getElementById("uf-id"),
  lat: document.getElementById("uf-lat"),
  lon: document.getElementById("uf-lon"),
  alt: document.getElementById("uf-alt"),
  gndAlt: document.getElementById("uf-gnd-alt"),
  hdg: document.getElementById("uf-hdg"),
  spd: document.getElementById("uf-spd"),
  curr: document.getElementById("uf-curr"),
  onGnd: document.getElementById("uf-on-gnd"),
};
function setUserAircraft(m) {
  const put = (el, v) => { el.textContent = v === null || v === undefined || v === "" ? "\u2014" : String(v); };
  userFields.id.textContent = m.id === null || m.id === undefined || m.id === "" ? "#\u2014" : "#" + m.id;
  put(userFields.lat, m.lat);
  put(userFields.lon, m.lon);
  put(userFields.alt, m.altFt);
  put(userFields.gndAlt, m.gndAltFt);
  put(userFields.hdg, m.hdgDeg);
  put(userFields.spd, m.gndSpdKt);
  put(userFields.onGnd, m.onGround ? "Yes" : "No");
  put(userFields.curr, m.curr);
  userChip.textContent = "Detected";
  userChip.classList.add("on");
}
document.getElementById("user-head").addEventListener("click", () => {
  document.getElementById("user-card").classList.toggle("collapsed");
});

/* ---- Aircrafts list (populated by the host via GSAirportUI) ---- */
const aircraftList = document.getElementById("aircraft-list");
const SVC_RANK = { inplace: 0, spawning: 1, none: 2 };
const aircrafts = new Map(); // id -> { data, service, collapsed, el }

function svcClass(s) { return s === "inplace" ? "inplace" : s === "spawning" ? "spawning" : ""; }
function fmt(v) { return v === null || v === undefined || v === "" ? "\u2014" : String(v); }

function fieldRow(label, value) {
  const row = document.createElement("div"); row.className = "ac-field";
  const k = document.createElement("span"); k.className = "k"; k.textContent = label;
  const v = document.createElement("span"); v.className = "v"; v.textContent = fmt(value);
  row.append(k, v); return row;
}

function groupBlock(title, rows) {
  const g = document.createElement("div"); g.className = "ac-group";
  const h = document.createElement("h3"); h.textContent = title; g.appendChild(h);
  rows.forEach(([label, value]) => g.appendChild(fieldRow(label, value)));
  return g;
}

function buildBody(rec) {
  const d = rec.data;
  const groups = document.createElement("div"); groups.className = "ac-groups";
  groups.appendChild(groupBlock("Identity", [["ID", d.id], ["ATC", d.atc], ["Airline", d.airline], ["Flight", d.flight], ["Model", d.title], ["Size", d.category]]));
  groups.appendChild(groupBlock("Position", [["Latitude", d.lat], ["Longitude", d.lon], ["Altitude (ft)", d.altFt], ["Ground alt (ft)", d.gndAltFt], ["Heading (deg)", d.hdgDeg], ["Gnd speed (kt)", d.gndSpdKt]]));
  const sec = (v) => (typeof v === "number" && v > 0) ? v : null;
  const onOff = (v) => v ? "On" : (v === 0 || v === false ? "Off" : "\u2014");
  const yesNo = (v) => v ? "Yes" : (v === 0 || v === false ? "No" : "\u2014");
  const routeGroup = groupBlock("Route", [["From/To", (d.from || "\u2014") + "/" + (d.to || "\u2014")], ["Current", d.curr], ["Parking", d.parking], ["State", d.state], ["ETD (s)", sec(d.etdSec)], ["ETA (s)", sec(d.etaSec)]]);
  routeGroup.classList.add("ac-g-route");
  groups.appendChild(routeGroup);
  groups.appendChild(groupBlock("Systems", [["Beacon Lights", onOff(d.beacon)], ["Nav Lights", onOff(d.nav)], ["Taxi Lights", onOff(d.taxi)], ["Parking Brakes", onOff(d.parkBrake)], ["On Ground", yesNo(d.onGround)]]));
  groups.appendChild(groupBlock("Aircraft", [["Wing span (m)", d.wingSpanM], ["Engines", d.engines], ["Pushback attached", d.pushbackAttached], ["Pushback wait", d.pushbackWait], ["Push contact X (m)", d.pushX], ["Push contact Z (m)", d.pushZ]]));
  if (Array.isArray(d.interactivePoints) && d.interactivePoints.length > 0) {
    const pointsGroup = groupBlock("Interactive points (" + d.interactivePoints.length + ")",
      d.interactivePoints.map((p, i) => [(i + 1) + ". " + fmt(p.type), p.x + ", " + p.y + ", " + p.z, fmt(p.hdg)]));
    pointsGroup.classList.add("ac-g-points");
    groups.appendChild(pointsGroup);
  }
  const body = document.createElement("div"); body.className = "ac-body"; body.appendChild(groups);
  return body;
}

function buildCard(rec) {
  const d = rec.data;
  const card = document.createElement("div"); card.className = "ac-card"; card.dataset.id = String(d.id);
  const head = document.createElement("div"); head.className = "ac-head";
  const chev = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  chev.setAttribute("class", "ac-chev"); chev.setAttribute("viewBox", "0 0 24 24");
  chev.setAttribute("fill", "none"); chev.setAttribute("stroke", "currentColor");
  chev.setAttribute("stroke-width", "2.5"); chev.setAttribute("stroke-linecap", "round"); chev.setAttribute("stroke-linejoin", "round");
  const pth = document.createElementNS("http://www.w3.org/2000/svg", "path");
  pth.setAttribute("d", "M6 9l6 6 6-6"); chev.appendChild(pth);
  const idChip = document.createElement("span"); idChip.className = "ac-id"; idChip.textContent = "#" + d.id;
  const name = document.createElement("span"); name.className = "ac-name";
  const atc = document.createElement("span"); atc.className = "ac-name-atc";
  atc.textContent = d.atc || d.flight || d.title || "\u2014"; name.appendChild(atc);
  if (d.title) { const model = document.createElement("span"); model.className = "ac-name-model"; model.textContent = d.title; name.appendChild(model); }
  const svc = document.createElement("span"); svc.className = "ac-svc " + svcClass(rec.service); svc.textContent = rec.service === "none" ? "No Ground Services" : "Ground Services";
  head.append(chev, idChip, name, svc);
  card.append(head, buildBody(rec));
  head.addEventListener("click", () => { rec.collapsed = !rec.collapsed; card.classList.toggle("collapsed", rec.collapsed); });
  if (rec.collapsed) card.classList.add("collapsed");
  return card;
}

function upsertAircraft(payload) {
  const id = payload.id;
  if (id === undefined || id === null) return;
  let rec = aircrafts.get(id);
  if (!rec) { rec = { data: {}, service: "none", collapsed: false, el: null }; aircrafts.set(id, rec); }
  Object.assign(rec.data, payload); rec.data.id = id;
  if (typeof payload.service === "string" && payload.service in SVC_RANK) rec.service = payload.service;
  const el = buildCard(rec);
  if (rec.el && rec.el.parentNode) rec.el.replaceWith(el); else aircraftList.appendChild(el);
  rec.el = el;
  const ordered = Array.from(aircrafts.values()).sort((a, b) => {
    const r = SVC_RANK[a.service] - SVC_RANK[b.service];
    return r !== 0 ? r : a.data.id - b.data.id;
  });
  ordered.forEach((x) => aircraftList.appendChild(x.el));
}

window.GSAirportUI = {
  setUserAircraft,
  addAircraft: (payload) => upsertAircraft(payload),
  updateAircraft: (payload) => upsertAircraft(payload),
  removeAircraft: (id) => {
    const rec = aircrafts.get(id);
    if (!rec) return;
    if (rec.el && rec.el.parentNode) rec.el.remove();
    aircrafts.delete(id);
  },
};

function routeUiMessage(line) {
  if (line.charCodeAt(0) !== 123) return false;
  let msg;
  try { msg = JSON.parse(line); } catch (e) { return false; }
  if (!msg || typeof msg !== "object" || typeof msg.op !== "string") return false;
  if (msg.op === "aircraft.add") { window.GSAirportUI.addAircraft(msg); return true; }
  if (msg.op === "aircraft.update") { window.GSAirportUI.updateAircraft(msg); return true; }
  if (msg.op === "aircraft.remove") { window.GSAirportUI.removeAircraft(msg.id); return true; }
  if (msg.op === "user.set") { window.GSAirportUI.setUserAircraft(msg); return true; }
  return false;
}

/* ---- WebView → JS messages ---- */
if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener("message", (event) => {
    if (typeof event.data !== "string") return;
    for (const line of event.data.split("\n")) {
      if (!line) continue;
      if (routeUiMessage(line)) continue;
      appendLogLine(line);
    }
  });
  // Tell the host it is safe to flush startup logs only after the listener exists.
  window.chrome.webview.postMessage("ready");
}
