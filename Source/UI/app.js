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
const statusPill = document.getElementById("status-pill");
const statusText = document.getElementById("status-text");
const versionChip = document.getElementById("version-chip");

const MAX_LOG_LINES = 2000;
let logLineCount = 0;
let sawBanner = false;

/* ---- Persistent UI state (the WebView2 profile survives across runs and
   across version updates - stored settings are kept as-is). The current UI
   version is recorded as well; it is not evaluated yet, but is kept for
   future use (e.g. diagnostics or migration logic). ---- */
(function recordVersion() {
  try {
    const version = versionChip ? versionChip.textContent.trim() : "";
    if (!version) return;
    localStorage.setItem("gsalive.version", version);
  } catch (e) {
    console.error("localStorage unavailable, version not recorded:", e);
  }
})();

function removePlaceholder() {
  if (logPlaceholder && logPlaceholder.parentNode) logPlaceholder.remove();
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


/* ---- Settings (persisted; the host is notified on load and on every change) ---- */
const INJECTOR_KEY = "gsalive.injector";
const GROUND_SERVICED_KEY = "gsalive.ground-serviced";
const injectorSelect = document.getElementById("select-injector");
const groundServicedRow = document.getElementById("ground-serviced-row");
const groundServicedSlider = document.getElementById("ground-serviced");
const groundServicedValue = document.getElementById("ground-serviced-value");
function sendInjectorSettings() {
  bridge.send("injector " + injectorSelect.value + " " + groundServicedSlider.value);
}
function updateGroundServicedVisibility() {
  groundServicedRow.classList.toggle("hidden", injectorSelect.value === "0");
}
function applyInjector(value, notifyHost = true) {
  injectorSelect.value = value;
  try {
    localStorage.setItem(INJECTOR_KEY, value);
  } catch (e) {
    console.error("localStorage unavailable, setting not persisted:", e);
  }
  updateGroundServicedVisibility();
  if (notifyHost) sendInjectorSettings();
}
function applyGroundServiced(value, notifyHost = true) {
  groundServicedSlider.value = value;
  groundServicedValue.value = value + "%";
  groundServicedValue.textContent = value + "%";
  try {
    localStorage.setItem(GROUND_SERVICED_KEY, value);
  } catch (e) {
    console.error("localStorage unavailable, setting not persisted:", e);
  }
  if (notifyHost) sendInjectorSettings();
}
(function initInjector() {
  let injector = "99";
  let groundServiced = "50";
  try {
    const stored = localStorage.getItem(INJECTOR_KEY);
    if (stored === "0" || stored === "1" || stored === "99") injector = stored;
    const storedGroundServiced = localStorage.getItem(GROUND_SERVICED_KEY);
    if (storedGroundServiced !== null) {
      const percentage = Number(storedGroundServiced);
      if (Number.isInteger(percentage) && percentage >= 0 && percentage <= 100) {
        groundServiced = String(percentage);
      }
    }
  } catch (e) { /* no persistence available - use the default */ }
  applyGroundServiced(groundServiced, false);
  applyInjector(injector, false);
})();
injectorSelect.addEventListener("change", () => applyInjector(injectorSelect.value));
groundServicedSlider.addEventListener("input", () => applyGroundServiced(groundServicedSlider.value));

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
  userChip.textContent = "Detected";
  userChip.classList.add("on");
}
function setUserAirport(m) {
  userFields.curr.textContent = fmt(m.curr);
}
function clearUserAircraft() {
  for (const [key, el] of Object.entries(userFields)) {
    if (key !== "curr") el.textContent = "\u2014";
  }
  userFields.id.textContent = "#\u2014";
  userChip.textContent = "Not Detected";
  userChip.classList.remove("on");
}
function setConnected(connected) {
  statusPill.classList.remove("disconnected");
  statusPill.classList.toggle("connected", connected);
  statusText.textContent = connected ? "Connected" : "Waiting for MSFS 2024\u2026";
}
document.getElementById("user-head").addEventListener("click", () => {
  document.getElementById("user-card").classList.toggle("collapsed");
});

/* ---- Aircrafts list (populated by the host via GSAirportUI) ---- */
const aircraftList = document.getElementById("aircraft-list");
const SVC_RANK = { inplace: 0, spawning: 1, none: 2 };
const SVC_KEYS = ["none", "spawning", "inplace"]; // C++ AircraftService wire value -> internal key
const aircrafts = new Map(); // id -> { data, service, collapsed, el }

function svcClass(s) { return s === "inplace" ? "inplace" : s === "spawning" ? "spawning" : ""; }
function fmt(v) { return v === null || v === undefined || v === "" ? "\u2014" : String(v); }

function fieldRow(label, value) {
  const row = document.createElement("div"); row.className = "ac-field";
  const k = document.createElement("span"); k.className = "k"; k.textContent = label;
  const v = document.createElement("span"); v.className = "v"; v.textContent = fmt(value);
  row.append(k, v); return row;
}

function compactField(label, value) {
  const field = document.createElement("span"); field.className = "ac-extra-field";
  const k = document.createElement("span"); k.className = "k"; k.textContent = label;
  const v = document.createElement("span"); v.className = "v"; v.textContent = fmt(value);
  field.append(k, v); return field;
}

function mainField(label, value) {
  const field = document.createElement("span"); field.className = "ac-main-field";
  const k = document.createElement("span"); k.className = "k"; k.textContent = label;
  const v = document.createElement("span"); v.className = "v"; v.textContent = fmt(value);
  field.append(k, v); return field;
}

function groupBlock(title, rows) {
  const g = document.createElement("div"); g.className = "ac-group";
  const h = document.createElement("h3"); h.textContent = title; g.appendChild(h);
  rows.forEach(([label, value]) => g.appendChild(fieldRow(label, value)));
  return g;
}

function buildBody(rec) {
  const d = rec.data;
  const sec = (v) => (typeof v === "number" && v > 0) ? v : null;
  const onOff = (v) => v ? "On" : (v === 0 || v === false ? "Off" : "\u2014");
  const yesNo = (v) => v ? "Yes" : (v === 0 || v === false ? "No" : "\u2014");
  const mainFields = document.createElement("div"); mainFields.className = "ac-main-fields";
  [["Gnd speed (kt)", d.gndSpdKt], ["Latitude", d.lat], ["From/To", (d.from || "\u2014") + "/" + (d.to || "\u2014")], ["Beacon Lights", onOff(d.beacon)],
   ["On Ground", yesNo(d.onGround)], ["Longitude", d.lon], ["Parking", d.parking], ["Nav Lights", onOff(d.nav)],
   ["Size", d.category], ["Altitude (ft)", d.altFt], ["State", d.state], ["Taxi Lights", onOff(d.taxi)],
   ["Ground alt (ft)", d.gndAltFt], ["Heading (deg)", d.hdgDeg], ["Parking Brakes", onOff(d.parkBrake)], ["Pushback attached", d.pushbackAttached]]
    .forEach(([label, value]) => mainFields.appendChild(mainField(label, value)));

  const extra = document.createElement("div"); extra.className = "ac-extra";
  if (rec.detailsCollapsed) extra.classList.add("collapsed");
  const extraHead = document.createElement("div"); extraHead.className = "ac-extra-head";
  const extraChev = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  extraChev.setAttribute("class", "ac-extra-chev"); extraChev.setAttribute("viewBox", "0 0 24 24");
  extraChev.setAttribute("fill", "none"); extraChev.setAttribute("stroke", "currentColor");
  extraChev.setAttribute("stroke-width", "2.5"); extraChev.setAttribute("stroke-linecap", "round"); extraChev.setAttribute("stroke-linejoin", "round");
  const extraPath = document.createElementNS("http://www.w3.org/2000/svg", "path");
  extraPath.setAttribute("d", "M6 9l6 6 6-6"); extraChev.appendChild(extraPath);
  const extraTitle = document.createElement("span"); extraTitle.textContent = "Additional Details";
  extraHead.append(extraChev, extraTitle);

  const extraBody = document.createElement("div"); extraBody.className = "ac-extra-body";
  const extraFields = document.createElement("div"); extraFields.className = "ac-extra-fields";
  [["Airline", d.airline], ["Flight", d.flight], ["Current", d.curr], ["ETD (s)", sec(d.etdSec)], ["ETA (s)", sec(d.etaSec)],
   ["Wing span (m)", d.wingSpanM], ["Engines", d.engines], ["Pushback wait", d.pushbackWait], ["Push contact X (m)", d.pushX], ["Push contact Z (m)", d.pushZ]]
    .forEach(([label, value]) => extraFields.appendChild(compactField(label, value)));
  const points = Array.isArray(d.interactivePoints) ? d.interactivePoints : [];
  const pointsGroup = document.createElement("div"); pointsGroup.className = "ac-interactive-points";
  const pointsTitle = document.createElement("h3"); pointsTitle.textContent = "Interactive points (" + points.length + ")";
  const pointsList = document.createElement("div"); pointsList.className = "ac-points-list";
  const pointFields = points.length > 0 ? points.map((p, i) => [(i + 1) + ". " + fmt(p.type), p.x + ", " + p.y + ", " + p.z]) : [["1.", "\u2014"]];
  pointFields.forEach(([label, value]) => pointsList.appendChild(compactField(label, value)));
  pointsGroup.append(pointsTitle, pointsList);
  extraBody.append(extraFields, pointsGroup); extra.append(extraHead, extraBody);
  extraHead.addEventListener("click", () => { rec.detailsCollapsed = !rec.detailsCollapsed; extra.classList.toggle("collapsed", rec.detailsCollapsed); });

  const body = document.createElement("div"); body.className = "ac-body"; body.append(mainFields, extra);
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
  const atcText = (d.atc || "").trim();
  const modelText = (d.title || "").trim();
  if (atcText) { const atc = document.createElement("span"); atc.className = "ac-name-atc"; atc.textContent = atcText; name.appendChild(atc); }
  if (modelText) { const model = document.createElement("span"); model.className = "ac-name-model"; model.textContent = modelText; name.appendChild(model); }
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
  if (!rec) { rec = { data: {}, service: "none", collapsed: false, detailsCollapsed: true, el: null }; aircrafts.set(id, rec); }
  Object.assign(rec.data, payload); rec.data.id = id;
  const el = buildCard(rec);
  if (rec.el && rec.el.parentNode) rec.el.replaceWith(el); else aircraftList.appendChild(el);
  rec.el = el;
  const ordered = Array.from(aircrafts.values()).sort((a, b) => {
    const r = SVC_RANK[a.service] - SVC_RANK[b.service];
    return r !== 0 ? r : a.data.id - b.data.id;
  });
  ordered.forEach((x) => aircraftList.appendChild(x.el));
}

function setAircraftServices(payload) {
  const id = payload.id;
  if (id === undefined || id === null) return;
  const rec = aircrafts.get(id);
  if (!rec) return;
  if (payload.service !== 0 && payload.service !== 1 && payload.service !== 2) return;
  rec.service = SVC_KEYS[payload.service];
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
  setUserAirport,
  clearUserAircraft,
  setConnected,
  addAircraft: (payload) => upsertAircraft(payload),
  updateAircraft: (payload) => upsertAircraft(payload),
  setAircraftServices,
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
  if (msg.op === "aircraft.services") { window.GSAirportUI.setAircraftServices(msg); return true; }
  if (msg.op === "user.set") { window.GSAirportUI.setUserAircraft(msg); return true; }
  if (msg.op === "user.airport") { window.GSAirportUI.setUserAirport(msg); return true; }
  if (msg.op === "user.clear") { window.GSAirportUI.clearUserAircraft(); return true; }
  if (msg.op === "status.set") { window.GSAirportUI.setConnected(msg.connected === true); return true; }
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
  // The host accepts commands only after it processes "ready".
  sendInjectorSettings();
}
