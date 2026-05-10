#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ==========================================
// CONFIGURATION
// ==========================================
const char* AP_SSID = "AStatine_Generator";
const char* AP_PASS = "12345678"; // Must be at least 8 characters
const int PWM_PIN = 2;            // GPIO 5 (D1 on NodeMCU/Wemos)

// Initial default state
int currentFreq = 1000; // 1 kHz
int currentDuty = 50;   // 50%

ESP8266WebServer server(80);

// ==========================================
// WEB DASHBOARD HTML/CSS/JS (AStatine Branded)
// ==========================================
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AStatine PWM Generator</title>
    <style>
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            background-color: #0f1115; 
            color: #ffffff; 
            text-align: center; 
            padding: 20px; 
            margin: 0;
        }
        h1 { 
            color: #00e5ff; 
            text-transform: uppercase; 
            letter-spacing: 3px; 
            margin-bottom: 5px;
        }
        .branding { 
            font-size: 0.9em; 
            color: #666; 
            margin-bottom: 35px; 
            letter-spacing: 1px;
        }
        .container { 
            max-width: 450px; 
            margin: 0 auto; 
            background: #1a1d24; 
            padding: 40px 30px; 
            border-radius: 12px; 
            box-shadow: 0 8px 32px rgba(0, 229, 255, 0.15); 
            border: 1px solid #2a2d35;
        }
        .control-group { 
            margin-bottom: 35px; 
            text-align: left; 
        }
        label { 
            display: flex; 
            justify-content: space-between; 
            align-items: center;
            font-weight: bold; 
            margin-bottom: 15px; 
            color: #b0bec5; 
            font-size: 1.1em;
        }
        input[type=range] { 
            width: 100%; 
            cursor: pointer; 
            accent-color: #00e5ff; 
            height: 8px;
            background: #2a2d35;
            border-radius: 4px;
            outline: none;
        }
        input[type=number] {
            font-family: 'Courier New', Courier, monospace; 
            color: #00e5ff; 
            font-size: 1em; 
            background: #0f1115;
            padding: 6px 10px;
            border-radius: 6px;
            border: 1px solid #2a2d35;
            width: 100px;
            text-align: right;
            outline: none;
        }
        input[type=number]:focus {
            border-color: #00e5ff;
        }
        .value-display { 
            font-family: 'Courier New', Courier, monospace; 
            color: #00e5ff; 
            font-size: 1.1em; 
            background: #0f1115;
            padding: 4px 10px;
            border-radius: 6px;
            min-width: 60px;
            text-align: right;
        }
        .log-container {
            margin-top: 20px;
            background: #0a0c0f;
            border: 1px solid #2a2d35;
            border-radius: 8px;
            padding: 10px;
            height: 120px;
            overflow-y: auto;
            text-align: left;
            font-family: 'Courier New', Courier, monospace;
            font-size: 0.85em;
        }
        .log-entry { margin-bottom: 4px; }
        .log-time { color: #666; margin-right: 8px; }
        .log-success { color: #4caf50; }
        .log-error { color: #ff5252; }
        .log-info { color: #00e5ff; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AStatine</h1>
        <div class="branding">PRECISION FUNCTION GENERATOR</div>

        <div class="control-group">
            <label>
                <span>Frequency (Hz)</span> 
                <input type="number" id="freqManual" value="1000" min="1" max="100000" onchange="syncInput('manual')" onkeyup="if(event.key === 'Enter') syncInput('manual')">
            </label>
            <input type="range" id="freqSlider" min="1" max="100000" value="1000" oninput="syncInput('slider')" onchange="sendData()">
        </div>

        <div class="control-group">
            <label>
                <span>Duty Cycle</span> 
                <span class="value-display" id="dutyVal">50 %</span>
            </label>
            <input type="range" id="dutySlider" min="0" max="100" value="50" oninput="updateDutyText()" onchange="sendData()">
        </div>

        <div class="log-container" id="liveLog">
            <div class="log-entry"><span class="log-time">[System]</span><span class="log-info">AStatine Dashboard Initialized.</span></div>
        </div>
    </div>

    <script>
        // --- Logging System ---
        function writeLog(msg, type = 'info') {
            const logBox = document.getElementById('liveLog');
            const now = new Date();
            const timeStr = now.getHours().toString().padStart(2, '0') + ':' + 
                            now.getMinutes().toString().padStart(2, '0') + ':' + 
                            now.getSeconds().toString().padStart(2, '0');
            
            let colorClass = 'log-info';
            if (type === 'error') colorClass = 'log-error';
            if (type === 'success') colorClass = 'log-success';

            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.innerHTML = `<span class="log-time">[${timeStr}]</span><span class="${colorClass}">${msg}</span>`;
            
            logBox.appendChild(entry);
            logBox.scrollTop = logBox.scrollHeight; // Auto-scroll to bottom
        }

        // --- UI Synchronization ---
        function updateDutyText() {
            document.getElementById('dutyVal').innerText = document.getElementById('dutySlider').value + ' %';
        }

        function syncInput(source) {
            let slider = document.getElementById('freqSlider');
            let manual = document.getElementById('freqManual');
            
            if (source === 'slider') {
                manual.value = slider.value;
            } else if (source === 'manual') {
                let val = parseInt(manual.value);
                
                // Hardware limit bounds checking
                if (isNaN(val) || val < 1) {
                    val = 1;
                    writeLog("Error: Minimum frequency is 1 Hz.", "error");
                } else if (val > 100000) {
                    val = 100000;
                    writeLog("Warning: Max stable hardware limit (100kHz) reached.", "error");
                }
                
                manual.value = val;
                slider.value = val;
                sendData(); // Trigger send when manual input is finalized
            }
        }

        // --- Hardware Communication ---
        function sendData() {
            let f = document.getElementById('freqSlider').value;
            let d = document.getElementById('dutySlider').value;
            
            fetch(`/set?freq=${f}&duty=${d}`)
                .then(response => {
                    if(response.ok) {
                        writeLog(`Output updated: ${f} Hz, ${d}% Duty`, "success");
                    } else {
                        writeLog(`Hardware rejected parameters (HTTP ${response.status})`, "error");
                    }
                })
                .catch(err => {
                    writeLog("Connection Error: Is the ESP8266 still in range?", "error");
                });
        }
    </script>
</body>
</html>
)=====";

// ==========================================
// SERVER ROUTING & HARDWARE CONTROL
// ==========================================

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleSet() {
    if (server.hasArg("freq") && server.hasArg("duty")) {
        int targetFreq = server.arg("freq").toInt();
        int targetDuty = server.arg("duty").toInt();

        // Server-side bounds checking for safety
        if(targetFreq < 1 || targetFreq > 1000000 || targetDuty < 0 || targetDuty > 100) {
            server.send(400, "text/plain", "Error: Values out of bounds");
            return;
        }

        currentFreq = targetFreq;
        currentDuty = targetDuty;

        // Apply hardware changes
        analogWriteFreq(currentFreq);
        int pwmValue = map(currentDuty, 0, 100, 0, 1023);
        analogWrite(PWM_PIN, pwmValue);

        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request: Missing arguments.");
    }
}

// ==========================================
// SETUP & LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    
    // Initialize PWM Pin
    pinMode(PWM_PIN, OUTPUT);
    analogWriteFreq(currentFreq);
    analogWrite(PWM_PIN, map(currentDuty, 0, 100, 0, 1023));

    // Initialize WiFi Access Point
    Serial.println("\nStarting AStatine Access Point...");
    WiFi.softAP(AP_SSID, AP_PASS);
    
    Serial.print("Connect to Wi-Fi network: ");
    Serial.println(AP_SSID);
    Serial.print("Open this IP in your browser: http://");
    Serial.println(WiFi.softAPIP());

    // Initialize Web Server
    server.on("/", HTTP_GET, handleRoot);
    server.on("/set", HTTP_GET, handleSet);
    server.begin();
    Serial.println("AStatine HTTP server started successfully.");
}

void loop() {
    // Keep the web server listening
    server.handleClient();
}
