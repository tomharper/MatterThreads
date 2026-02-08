#include "metrics/DashboardServer.h"
#include "core/Log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <algorithm>

namespace mt {

// ── Inline HTML (compiled-in from dashboard.html) ───────────────────────────

static const char* const DASHBOARD_HTML = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MatterThreads Dashboard</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  background: #1a1a2e; color: #e0e0e0; font-family: 'Menlo', 'Consolas', monospace;
  font-size: 13px; padding: 16px;
}
h1 { color: #00d4ff; font-size: 18px; margin-bottom: 16px; }
h2 { color: #7fc8f8; font-size: 14px; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px; }
.panel {
  background: #16213e; border: 1px solid #2a3a5c; border-radius: 8px; padding: 14px;
}
.full-width { grid-column: 1 / -1; }
.node-cards { display: flex; gap: 10px; flex-wrap: wrap; }
.node-card {
  flex: 1; min-width: 140px; background: #0f3460; border-radius: 6px; padding: 10px;
  border-left: 4px solid #555;
}
.node-card.running { border-left-color: #4CAF50; }
.node-card.stopped { border-left-color: #F44336; }
.node-card .role { font-size: 11px; color: #888; text-transform: uppercase; }
.node-card .id { font-size: 16px; font-weight: bold; color: #fff; }
.node-card .state { font-size: 11px; margin-top: 4px; }
.node-card .state.running { color: #4CAF50; }
.node-card .state.stopped { color: #F44336; }
.node-card .pid { font-size: 10px; color: #666; }
.topo-table { border-collapse: collapse; width: 100%; }
.topo-table th, .topo-table td {
  padding: 6px 8px; text-align: center; border: 1px solid #2a3a5c; font-size: 11px;
}
.topo-table th { background: #0f3460; color: #7fc8f8; }
.topo-cell { border-radius: 3px; padding: 4px; }
.topo-cell.good { background: #1b5e20; color: #a5d6a7; }
.topo-cell.degraded { background: #e65100; color: #ffcc80; }
.topo-cell.down { background: #b71c1c; color: #ef9a9a; }
.topo-cell.self { background: #333; color: #666; }
.metric-row { display: flex; justify-content: space-between; padding: 3px 0; border-bottom: 1px solid #1a1a2e; }
.metric-name { color: #aaa; }
.metric-value { color: #fff; font-weight: bold; }
.hist-bar-container { display: flex; align-items: center; gap: 8px; padding: 3px 0; }
.hist-bar { height: 14px; border-radius: 2px; min-width: 2px; }
.hist-bar.p50 { background: #4CAF50; }
.hist-bar.p95 { background: #FF9800; }
.hist-bar.p99 { background: #F44336; }
.hist-legend { display: flex; gap: 12px; margin-top: 6px; font-size: 10px; }
.hist-legend span::before { content: ''; display: inline-block; width: 8px; height: 8px; border-radius: 2px; margin-right: 4px; }
.legend-p50::before { background: #4CAF50 !important; }
.legend-p95::before { background: #FF9800 !important; }
.legend-p99::before { background: #F44336 !important; }
.timeline-list { max-height: 300px; overflow-y: auto; }
.timeline-entry { padding: 3px 0; border-bottom: 1px solid #1e2a45; font-size: 11px; display: flex; gap: 8px; }
.timeline-time { color: #666; min-width: 80px; }
.timeline-node { color: #00d4ff; min-width: 50px; }
.timeline-cat { color: #FF9800; min-width: 80px; }
.timeline-event { color: #e0e0e0; }
.timeline-detail { color: #888; }
.power-indicator { display: inline-block; padding: 4px 12px; border-radius: 4px; font-weight: bold; font-size: 12px; }
.power-indicator.EngineOn { background: #4CAF50; color: #fff; }
.power-indicator.ShuttingDown { background: #FF9800; color: #fff; }
.power-indicator.Off { background: #F44336; color: #fff; }
.power-indicator.Booting { background: #2196F3; color: #fff; }
.refresh-bar { position: fixed; top: 0; left: 0; height: 2px; background: #00d4ff; transition: width 2s linear; }
.status-line { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
.last-update { font-size: 10px; color: #555; }
</style>
</head>
<body>
<div class="refresh-bar" id="refreshBar"></div>
<div class="status-line">
  <h1>MatterThreads Dashboard</h1>
  <span class="last-update" id="lastUpdate">connecting...</span>
</div>
<div class="grid">
  <div class="panel full-width">
    <h2>Node Status</h2>
    <div class="node-cards" id="nodeCards">
      <div class="node-card"><div class="role">loading...</div></div>
    </div>
  </div>
  <div class="panel">
    <h2>Topology (Link Quality)</h2>
    <div id="topoGrid">loading...</div>
  </div>
  <div class="panel">
    <h2>Metrics</h2>
    <div id="counters">loading...</div>
    <div style="margin-top: 10px;">
      <h2>Histograms</h2>
      <div class="hist-legend">
        <span class="legend-p50">p50</span>
        <span class="legend-p95">p95</span>
        <span class="legend-p99">p99</span>
      </div>
      <div id="histograms"></div>
    </div>
  </div>
  <div class="panel full-width">
    <h2>Timeline</h2>
    <div class="timeline-list" id="timeline">loading...</div>
  </div>
</div>
<script>
const ROLES = ['Leader/BR', 'Router/Relay', 'EndDevice/Sensor', 'Phone'];
const ROLE_COLORS = ['#2196F3', '#FF9800', '#4CAF50', '#9C27B0'];

function updateNodes(nodes) {
  const el = document.getElementById('nodeCards');
  if (!nodes || nodes.length === 0) {
    el.innerHTML = '<div class="node-card"><div class="role">no data</div></div>';
    return;
  }
  el.innerHTML = nodes.map(n => `
    <div class="node-card ${n.state}">
      <div class="role">${n.role}</div>
      <div class="id" style="color:${ROLE_COLORS[n.id] || '#fff'}">Node ${n.id}</div>
      <div class="state ${n.state}">${n.state}</div>
      <div class="pid">PID ${n.pid}</div>
    </div>
  `).join('');
}

function updateTopology(matrix) {
  const el = document.getElementById('topoGrid');
  if (!matrix) { el.innerHTML = 'no data'; return; }
  const n = matrix.length;
  let html = '<table class="topo-table"><tr><th></th>';
  for (let i = 0; i < n; i++) html += '<th>N' + i + '</th>';
  html += '</tr>';
  for (let i = 0; i < n; i++) {
    html += '<tr><th>N' + i + '</th>';
    for (let j = 0; j < n; j++) {
      if (i === j) {
        html += '<td><div class="topo-cell self">-</div></td>';
      } else {
        const link = matrix[i][j];
        let cls = 'good';
        let text = (link.loss * 100).toFixed(0) + '%';
        if (!link.up) { cls = 'down'; text = 'DOWN'; }
        else if (link.loss > 0.1) { cls = 'degraded'; }
        html += '<td><div class="topo-cell ' + cls + '">' + text + '<br>' + link.latency.toFixed(0) + 'ms</div></td>';
      }
    }
    html += '</tr>';
  }
  html += '</table>';
  el.innerHTML = html;
}

function updateCounters(counters) {
  const el = document.getElementById('counters');
  if (!counters || Object.keys(counters).length === 0) {
    el.innerHTML = '<div class="metric-row"><span class="metric-name">no counters</span></div>';
    return;
  }
  el.innerHTML = Object.entries(counters).map(function(e) {
    return '<div class="metric-row"><span class="metric-name">' + e[0] + '</span><span class="metric-value">' + e[1] + '</span></div>';
  }).join('');
}

function updateHistograms(histograms) {
  const el = document.getElementById('histograms');
  if (!histograms || Object.keys(histograms).length === 0) {
    el.innerHTML = '<div class="metric-row"><span class="metric-name">no histograms</span></div>';
    return;
  }
  const maxVal = Math.max.apply(null, Object.values(histograms).map(function(h) { return h.p99 || 0; }).concat([1]));
  el.innerHTML = Object.entries(histograms).map(function(e) {
    const name = e[0], h = e[1];
    const scale = 200 / maxVal;
    return '<div style="margin-top:6px"><div class="metric-name">' + name + ' (n=' + h.count + ')</div>' +
      '<div class="hist-bar-container">' +
      '<div class="hist-bar p50" style="width:' + Math.max(2, h.p50 * scale) + 'px"></div>' +
      '<div class="hist-bar p95" style="width:' + Math.max(2, h.p95 * scale) + 'px"></div>' +
      '<div class="hist-bar p99" style="width:' + Math.max(2, h.p99 * scale) + 'px"></div>' +
      '<span style="font-size:10px;color:#888">p50=' + h.p50.toFixed(1) + ' p95=' + h.p95.toFixed(1) + ' p99=' + h.p99.toFixed(1) + '</span>' +
      '</div></div>';
  }).join('');
}

function updateTimeline(events) {
  const el = document.getElementById('timeline');
  if (!events || events.length === 0) {
    el.innerHTML = '<div class="timeline-entry"><span class="timeline-event">no events</span></div>';
    return;
  }
  const recent = events.slice(-100).reverse();
  el.innerHTML = recent.map(function(e) {
    return '<div class="timeline-entry">' +
      '<span class="timeline-time">' + (e.time_ms != null ? e.time_ms + 'ms' : '') + '</span>' +
      '<span class="timeline-node">N' + e.node + '</span>' +
      '<span class="timeline-cat">' + e.category + '</span>' +
      '<span class="timeline-event">' + e.event + '</span>' +
      '<span class="timeline-detail">' + (e.detail || '') + '</span></div>';
  }).join('');
}

function fetchJson(path) {
  return fetch(path).then(function(r) {
    if (!r.ok) return null;
    return r.json();
  }).catch(function() { return null; });
}

function refresh() {
  var bar = document.getElementById('refreshBar');
  bar.style.width = '0%';
  bar.style.transition = 'none';
  void bar.offsetWidth;
  bar.style.transition = 'width 2s linear';
  bar.style.width = '100%';

  Promise.all([
    fetchJson('/api/status'),
    fetchJson('/api/metrics'),
    fetchJson('/api/timeline'),
    fetchJson('/api/topology')
  ]).then(function(results) {
    if (results[0]) updateNodes(results[0].nodes);
    if (results[1]) {
      updateCounters(results[1].counters);
      updateHistograms(results[1].histograms);
    }
    if (results[2]) updateTimeline(results[2].events || results[2]);
    if (results[3]) updateTopology(results[3].links);
    document.getElementById('lastUpdate').textContent =
      'Updated: ' + new Date().toLocaleTimeString();
  });
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>)HTML";

// ── DashboardServer implementation ──────────────────────────────────────────

DashboardServer::DashboardServer(const Collector& collector, uint16_t port)
    : collector_(collector), reporter_(collector), port_(port)
{
}

Result<void> DashboardServer::start() {
    auto result = Socket::listen(port_);
    if (!result) return Error("Dashboard server bind failed on port " +
                               std::to_string(port_) + ": " + result.error().message);

    listen_socket_ = std::move(*result);
    auto nb = listen_socket_.setNonBlocking(true);
    if (!nb) return Error("Failed to set non-blocking: " + nb.error().message);

    running_ = true;
    MT_INFO("dashboard", "Listening on http://localhost:" + std::to_string(port_));
    return Result<void>::success();
}

void DashboardServer::poll() {
    if (!running_) return;

    auto result = listen_socket_.accept();
    if (!result) {
        // EAGAIN/EWOULDBLOCK is expected in non-blocking mode — no pending connection
        return;
    }

    handleClient(std::move(*result));
}

void DashboardServer::stop() {
    running_ = false;
    listen_socket_.close();
}

void DashboardServer::handleClient(Socket client) {
    // Read request (up to 4KB — we only need the request line)
    char buf[4096];
    ssize_t n = ::recv(client.fd(), buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    handleRequest(client, std::string(buf, static_cast<size_t>(n)));
}

void DashboardServer::handleRequest(Socket& client, const std::string& request) {
    // Parse request line: "GET /path HTTP/1.1\r\n..."
    std::string method, path;
    std::istringstream iss(request);
    iss >> method >> path;

    if (method != "GET") {
        sendResponse(client, 405, "text/plain", "Method Not Allowed");
        return;
    }

    if (path == "/" || path == "/index.html") {
        sendResponse(client, 200, "text/html; charset=utf-8", DASHBOARD_HTML);
    } else if (path == "/api/status") {
        sendResponse(client, 200, "application/json", buildStatusJson());
    } else if (path == "/api/metrics") {
        sendResponse(client, 200, "application/json", reporter_.summaryJson());
    } else if (path == "/api/timeline") {
        sendResponse(client, 200, "application/json", collector_.timeline().exportJson());
    } else if (path == "/api/topology") {
        sendResponse(client, 200, "application/json", buildTopologyJson());
    } else {
        sendNotFound(client);
    }
}

void DashboardServer::sendResponse(Socket& client, int status_code,
                                    const std::string& content_type,
                                    const std::string& body) {
    std::string status_text;
    switch (status_code) {
        case 200: status_text = "OK"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        default: status_text = "Error"; break;
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    std::string resp = response.str();
    // Best-effort send — client might disconnect
    ::send(client.fd(), resp.data(), resp.size(), 0);
}

void DashboardServer::sendNotFound(Socket& client) {
    sendResponse(client, 404, "text/plain", "Not Found");
}

std::string DashboardServer::buildStatusJson() const {
    nlohmann::json j;
    nlohmann::json nodes_arr = nlohmann::json::array();

    if (node_status_provider_) {
        auto nodes = node_status_provider_();
        for (const auto& n : nodes) {
            nodes_arr.push_back({
                {"id", n.id},
                {"role", n.role},
                {"state", n.state},
                {"pid", n.pid}
            });
        }
    }

    j["nodes"] = nodes_arr;
    return j.dump();
}

std::string DashboardServer::buildTopologyJson() const {
    nlohmann::json j;

    if (topology_provider_) {
        auto matrix = topology_provider_();
        nlohmann::json links = nlohmann::json::array();
        for (size_t i = 0; i < 4; ++i) {
            nlohmann::json row = nlohmann::json::array();
            for (size_t k = 0; k < 4; ++k) {
                const auto& link = matrix[i][k];
                row.push_back({
                    {"loss", link.loss_rate},
                    {"latency", link.latency_mean_ms},
                    {"up", link.link_up},
                    {"lqi", link.lqi},
                    {"rssi", link.rssi}
                });
            }
            links.push_back(row);
        }
        j["links"] = links;
    } else {
        j["links"] = nullptr;
    }

    return j.dump();
}

const std::string& DashboardServer::dashboardHtml() {
    static const std::string html(DASHBOARD_HTML);
    return html;
}

} // namespace mt
