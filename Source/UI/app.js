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
    if (versionMatch) versionChip.textContent = "v" + versionMatch[1];
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

/* ---- WebView → JS messages ---- */
if (window.chrome && window.chrome.webview) {
  window.chrome.webview.addEventListener("message", (event) => {
    if (typeof event.data !== "string") return;
    for (const line of event.data.split("\n")) {
      if (line) appendLogLine(line);
    }
  });
  // Tell the host it is safe to flush startup logs only after the listener exists.
  window.chrome.webview.postMessage("ready");
}
