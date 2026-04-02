#define VEHICLE_TWAI_RX_PIN 34
#define VEHICLE_TWAI_TX_PIN 33

#include <Arduino.h>
#include <vehicle.hpp>
#include <SD.h>
#include <Ticker.h>

#include <WiFi.h>
#include <WebServer.h>


#define RL 22 //red light
#define HL 16 //hazard left
#define HR 17 //hazard right 

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const char *ssid = "DukeOpen";
const char *password = "";
WebServer server(80);


void send_all(void *args) {
  while(true) {
    g_vehicle.send_all();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool hazard = false;
bool leftTurn = false;
bool rightTurn = false;
bool backrunningLights = false;
bool brakeLights = false;


bool sdReady = false;

//void logVehicleState();


void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Vehicle Dashboard</title>
  <style>
    body { font-family: monospace; background: #111; color: #0f0; padding: 20px; }
    h1 { color: #fff; }
    .card { background: #222; border: 1px solid #0f0; border-radius: 6px; padding: 16px; margin: 8px 0; }
    .label { color: #aaa; font-size: 0.85em; }
    .value { font-size: 1.5em; font-weight: bold; }
    .on  { color: #0f0; }
    .off { color: #555; }
  </style>
</head>
<body>
  <h1>Vehicle Dashboard</h1>
  <div class="card"><span class="label">Voltage</span><br><span class="value" id="voltage">--</span> V</div>
  <div class="card"><span class="label">Current</span><br><span class="value" id="current">--</span> A</div>
  <div class="card"><span class="label">Motor RPM</span><br><span class="value" id="rpm">--</span></div>
  <div class="card"><span class="label">Throttle Avg</span><br><span class="value" id="throttle_avg">--</span></div>
  <div class="card"><span class="label">Throttle Raw</span><br><span class="value" id="throttle_raw">--</span></div>
  <div class="card">
    <span class="label">Lights</span><br>
    <span id="brake">Brake</span> &nbsp;
    <span id="backrun">Back Running</span> &nbsp;
    <span id="hazard">Hazard</span> &nbsp;
    <span id="left">Left Turn</span> &nbsp;
    <span id="right">Right Turn</span>
  </div>
  <script>
    function update() {
      fetch('/data').then(r => r.json()).then(d => {
        document.getElementById('voltage').textContent      = d.voltage;
        document.getElementById('current').textContent      = d.current;
        document.getElementById('rpm').textContent          = d.rpm;
        document.getElementById('throttle_avg').textContent = d.throttle_avg;
        document.getElementById('throttle_raw').textContent = d.throttle_raw;
        ['brake','backrun','hazard','left','right'].forEach(k => {
          const el = document.getElementById(k);
          el.className = d[k] ? 'on' : 'off';
        });
      });
    }
    update();
    setInterval(update, 500);
  </script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

void handleGet() {
  String json = "{";
  json += "\"voltage\":"      + String(g_vehicle.m_pdb_voltage)      + ",";
  json += "\"current\":"      + String(g_vehicle.m_pdb_current)      + ",";
  json += "\"rpm\":"          + String(g_vehicle.m_motor_rpm)         + ",";
  json += "\"throttle_avg\":" + String(g_vehicle.m_throttle_average) + ",";
  json += "\"throttle_raw\":" + String(g_vehicle.m_throttle_raw)     + ",";
  json += "\"brake\":"        + String(brakeLights ? "true" : "false")      + ",";
  json += "\"backrun\":"      + String(backrunningLights ? "true" : "false") + ",";
  json += "\"hazard\":"       + String(hazard ? "true" : "false")            + ",";
  json += "\"left\":"         + String(leftTurn ? "true" : "false")          + ",";
  json += "\"right\":"        + String(rightTurn ? "true" : "false")         + "}";
  server.send(200, "application/json", json);
}

void handlePost() {
 server.send(200, "text/plain", "Processing Data");
}



void handleUpload() {
 HTTPUpload& upload = server.upload();
 if (upload.status == UPLOAD_FILE_START) {
   Serial.println("Receiving data:");
 } else if (upload.status == UPLOAD_FILE_WRITE) {
   Serial.write(upload.buf, upload.currentSize);
 } else if (upload.status == UPLOAD_FILE_END) {
   server.send(200, "text/plain", "Data: ");
 }
}




void setup() {
  g_vehicle.init_network(DevBoard::COMMUNICATIONS);
  xTaskCreate(send_all, "Sending", 4096, nullptr, 5, nullptr);

  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HR, OUTPUT);
  pinMode(HL, OUTPUT);
  pinMode(RL, OUTPUT);


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");

  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Start the server
  server.on("/", handleRoot);
  server.on("/data", HTTP_GET, handleGet);
  server.on("/get", HTTP_GET, handleGet);
  server.on("/post", HTTP_POST, handlePost, handleUpload);
  server.begin();
}

void readStates() {
  if (g_vehicle.m_peripherals_backrunninglights == 1) {
    backrunningLights = true;
  } else {
    backrunningLights = false;
  }

  if(g_vehicle.m_peripherals_brakelights == 1) {
    brakeLights = true;
  } else {
    brakeLights = false;
  }

  if(g_vehicle.m_peripherals_hazard == 1) {
    hazard = true;
  } else {
    hazard = false;
  }

  if(g_vehicle.m_peripherals_turn == 2) {
    rightTurn = true;
  } else {
    rightTurn = false;
  }

  if(g_vehicle.m_peripherals_turn == 1) {
    leftTurn = true;
  } else {
    leftTurn = false;
  }

  
}


void printState() {
  //delay(1000);
  Serial.print("Voltage: ");
  Serial.println(g_vehicle.m_pdb_voltage);
  Serial.print("Current: ");
  Serial.println(g_vehicle.m_pdb_current);
  Serial.print("RPM: ");
  Serial.println(g_vehicle.m_motor_rpm);
  Serial.print("Throttle Avg: ");
  Serial.println(g_vehicle.m_throttle_average);
  Serial.print("Throttle Raw: ");
  Serial.println(g_vehicle.m_throttle_raw);
  
}

void loop() {
  // TODO: rest of device
  server.handleClient();
  readStates();

  if(hazard || leftTurn){
    digitalWrite(HL, HIGH);
  }

  if(hazard || rightTurn){
    digitalWrite(HR, HIGH);
  }

  if(backrunningLights) {
    analogWrite(RL,50);
  } 

  if(brakeLights) {
    analogWrite(RL, 255);
  } 

  //printState();

}
