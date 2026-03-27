#include "config_portal.h"

#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <algorithm>

#include "config.h"
#include "globals.h"
#include "device_config.h"

static WebServer server(CONFIG_PORTAL_PORT);
static bool portalRunning = false;
static String apSsid;
static bool restartScheduled = false;
static unsigned long restartAt = 0;

struct WifiNet {
  String ssid;
  int rssi;
  wifi_auth_mode_t enc;
};

static String HtmlEscape(const String& value) {
  String out = value;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  out.replace("'", "&#39;");
  return out;
}

static String BuildNetworksOptions() {
  String options;

  Serial.println("[PORTAL] Scanning WiFi networks...");

  WiFi.scanDelete();
  delay(100);

  int count = WiFi.scanNetworks(false, true);

  Serial.print("[PORTAL] Networks found: ");
  Serial.println(count);

  if (count <= 0) {
    options += "<option value=\"\">No se detectaron redes</option>";
    return options;
  }

  std::vector<WifiNet> nets;

  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    wifi_auth_mode_t enc = WiFi.encryptionType(i);

    Serial.print("[PORTAL] SSID: ");
    Serial.print(ssid);
    Serial.print(" | RSSI: ");
    Serial.print(rssi);
    Serial.print(" | ENC: ");
    Serial.println((int)enc);

    if (ssid.length() == 0) {
      continue;
    }

    // Evitar duplicados: dejar solo la mejor señal para cada SSID
    bool found = false;
    for (auto& n : nets) {
      if (n.ssid == ssid) {
        if (rssi > n.rssi) {
          n.rssi = rssi;
          n.enc = enc;
        }

        found = true;
        break;
      }
    }

    if (!found) {
      WifiNet net;
      net.ssid = ssid;
      net.rssi = rssi;
      net.enc = enc;
      nets.push_back(net);
    }
  }

  // Ordenar por mejor señal primero
  std::sort(nets.begin(), nets.end(), [](const WifiNet& a, const WifiNet& b) {
    return a.rssi > b.rssi;
  });

  for (const auto& n : nets) {
    String escaped = HtmlEscape(n.ssid);
    String label = escaped + " (RSSI " + String(n.rssi) + " dBm)";

    if (n.enc != WIFI_AUTH_OPEN) {
      label += " 🔒";
    }

    options += "<option value=\"" + escaped + "\">" + label + "</option>";
  }

  if (options.length() == 0) {
    options += "<option value=\"\">No se detectaron redes visibles</option>";
  }

  return options;
}

static String BuildPage(const String& message = "", bool isError = false) {
  String options = BuildNetworksOptions();

  String html;
  html.reserve(9000);

  html += "<!DOCTYPE html>";
  html += "<html lang='es'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Configurar WiFi ESP32</title>";
  html += "<style>";
  html += "body{font-family:Arial,Helvetica,sans-serif;background:#f4f6f8;margin:0;padding:20px;color:#222;}";
  html += ".box{max-width:560px;margin:0 auto;background:#fff;border-radius:12px;padding:24px;box-shadow:0 8px 25px rgba(0,0,0,0.08);}";
  html += "h1{margin-top:0;font-size:24px;}";
  html += ".muted{color:#666;font-size:14px;margin-bottom:20px;}";
  html += "label{display:block;margin-top:14px;margin-bottom:6px;font-weight:bold;}";
  html += "select,input{width:100%;padding:12px;border:1px solid #cfd8dc;border-radius:8px;box-sizing:border-box;font-size:15px;}";
  html += "button{margin-top:20px;background:#1976d2;color:#fff;border:none;padding:12px 16px;border-radius:8px;font-size:15px;cursor:pointer;width:100%;}";
  html += ".msg{margin-bottom:16px;padding:12px;border-radius:8px;}";
  html += ".ok{background:#e8f5e9;color:#256029;}";
  html += ".err{background:#ffebee;color:#c62828;}";
  html += ".small{font-size:13px;color:#666;margin-top:8px;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='box'>";
  html += "<h1>Configuración WiFi</h1>";
  html += "<div class='muted'>Dispositivo: <strong>" + HtmlEscape(deviceId) + "</strong></div>";

  if (message.length() > 0) {
    html += "<div class='msg ";
    html += isError ? "err" : "ok";
    html += "'>";
    html += HtmlEscape(message);
    html += "</div>";
  }

  html += "<form method='POST' action='/save'>";

  html += "<label for='ssid_select'>Red detectada</label>";
  html += "<select id='ssid_select' name='ssid_select'>";
  html += "<option value=''>-- Seleccione una red visible --</option>";
  html += options;
  html += "</select>";

  html += "<label for='ssid_manual'>O escribir SSID manualmente</label>";
  html += "<input id='ssid_manual' name='ssid_manual' type='text' maxlength='64' placeholder='Nombre de la red WiFi'>";

  html += "<label for='password'>Clave WiFi</label>";
  html += "<input id='password' name='password' type='password' maxlength='64' placeholder='Clave de la red'>";

  html += "<button type='submit'>Guardar y conectar</button>";
  html += "</form>";

  html += "<div class='small'>Las redes repetidas se agrupan mostrando la de mejor señal.</div>";
  html += "<div class='small'>Si completas SSID manual, se usará ese valor; si no, se tomará la red seleccionada.</div>";
  html += "<div class='small'>Luego de guardar, el equipo reiniciará e intentará conectarse a la red indicada.</div>";
  html += "</div>";
  html += "</body>";
  html += "</html>";

  return html;
}

static void HandleRoot() {
  server.send(200, "text/html; charset=utf-8", BuildPage());
}

static void HandleSave() {
  String ssidManual = server.arg("ssid_manual");
  String ssidSelect = server.arg("ssid_select");
  String password = server.arg("password");

  ssidManual.trim();
  ssidSelect.trim();
  password.trim();

  String finalSsid = ssidManual.length() > 0 ? ssidManual : ssidSelect;

  Serial.print("[PORTAL] Save requested. SSID selected/manual = ");
  Serial.println(finalSsid);

  if (finalSsid.length() == 0) {
    server.send(400, "text/html; charset=utf-8",
                BuildPage("Debe seleccionar una red o escribir el SSID manualmente.", true));
    return;
  }

  bool saved = DeviceConfig_SaveWifiConfig(finalSsid, password);
  if (!saved) {
    server.send(500, "text/html; charset=utf-8",
                BuildPage("No fue posible guardar la configuración WiFi.", true));
    return;
  }

  Serial.println("[PORTAL] WiFi configuration saved successfully");

  server.send(200, "text/html; charset=utf-8",
              BuildPage("Configuración guardada correctamente. El dispositivo reiniciará en unos segundos.", false));

  restartScheduled = true;
  restartAt = millis() + 2500UL;
}

static void HandleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void StartConfigPortal() {
  if (portalRunning) {
    return;
  }

  WiFi.disconnect(true, true);
  delay(300);

  WiFi.mode(WIFI_AP_STA);
  delay(200);

  apSsid = "ESP32-SETUP-" + deviceId;

  bool apOk;
  if (String(CONFIG_PORTAL_AP_PASSWORD).length() > 0) {
    apOk = WiFi.softAP(apSsid.c_str(), CONFIG_PORTAL_AP_PASSWORD);
  } else {
    apOk = WiFi.softAP(apSsid.c_str());
  }

  if (!apOk) {
    Serial.println("[PORTAL] Failed to start AP");
    return;
  }

  delay(500);

  server.on("/", HTTP_GET, HandleRoot);
  server.on("/save", HTTP_POST, HandleSave);
  server.onNotFound(HandleNotFound);
  server.begin();

  portalRunning = true;
  restartScheduled = false;

  Serial.println("[PORTAL] Configuration portal started");
  Serial.print("[PORTAL] AP SSID: ");
  Serial.println(apSsid);
  Serial.print("[PORTAL] AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void LoopConfigPortal() {
  if (!portalRunning) {
    return;
  }

  server.handleClient();

  if (restartScheduled && millis() >= restartAt) {
    Serial.println("[PORTAL] Restarting device...");
    delay(200);
    ESP.restart();
  }
}

bool IsConfigPortalRunning() {
  return portalRunning;
}