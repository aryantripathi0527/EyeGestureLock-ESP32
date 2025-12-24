#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

/* ========== WIFI DETAILS ========== */
const char* ssid = "Ritu";
const char* password = "12345678";

/* ========== PINS ========== */
#define EOG_PIN 1
#define LED_PIN 13

/* ========== PASSWORD (LEFT=-1, RIGHT=1) ========== */
int storedPassword[3] = { -1, -1, -1 }; // LEFT, LEFT, LEFT

/* ========== VARIABLES ========== */
int inputSeq[3];
int indexSeq = 0;

int baseline = 2048;
int leftThr = 1800;
int rightThr = 2200;

bool invertPolarity = false;      // set true if swapping temples makes things worse
unsigned long lastGestureMs = 0;
const unsigned long refractoryMs = 700;  // ignore new gesture immediately after one
const int deadband = 20;                 // ignore tiny deviations around baseline

String authStatus = "Waiting";
String gestureLog = "";
unsigned long accessTime = 0;

/* ========== WEB SERVER ========== */
WebServer server(80);

/* ========== FRONTEND HTML ========== */
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Eye Gesture Lock</title>
<style>
body{background:#0f172a;color:white;font-family:Arial;text-align:center;}
.card{background:#1e293b;width:360px;margin:80px auto;padding:25px;border-radius:16px;}
.lock{font-size:70px;}
.status{font-size:22px;margin-top:10px;}
.log{font-size:18px;margin-top:10px;color:#94a3b8;}
</style>
</head>
<body>
<div class="card">
<h2>👁️ Eye Gesture Lock</h2>
<div class="lock" id="lock">🔒</div>
<div class="status" id="status">Waiting</div>
<div class="log" id="log">No gestures yet</div>
</div>
<script>
setInterval(()=>{
fetch("/status")
.then(r=>r.text())
.then(t=>{
document.getElementById("status").innerHTML=t;
document.getElementById("lock").innerHTML =
t.includes("GRANTED") ? "🔓" : "🔒";
});
fetch("/log")
.then(r=>r.text())
.then(t=>{
document.getElementById("log").innerHTML = "Gestures: " + t;
});
},1000);
</script>
</body>
</html>
)rawliteral";

/* ========== FILTERING (moving average) ========== */
#define FILTER_SIZE 5
int filterBuf[FILTER_SIZE];
int filterIdx = 0;

int filteredRead(int pin) {
  filterBuf[filterIdx] = analogRead(pin);
  filterIdx = (filterIdx + 1) % FILTER_SIZE;
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += filterBuf[i];
  return sum / FILTER_SIZE;
}

/* ========== GUIDED CALIBRATION WITH EXTREMES ========== */
void calibrate() {
  // 1) Straight: baseline
  Serial.println("Calibration: Look STRAIGHT (3s)");
  delay(1000);
  long sum = 0; int valid = 0;
  for (int i = 0; i < 150; i++) {
    int v = filteredRead(EOG_PIN);
    if (v > 500 && v < 3500) { sum += v; valid++; }
    delay(10);
  }
  baseline = (valid > 50) ? (sum / valid) : 2048;
  Serial.print("Baseline: "); Serial.println(baseline);

  // 2) LEFT: hold left, collect min
  Serial.println("Calibration: Look LEFT and hold (2s)");
  delay(500);
  int leftMin = 4095;
  for (int i = 0; i < 120; i++) {
    int v = filteredRead(EOG_PIN);
    if (v > 500 && v < 3500) leftMin = min(leftMin, v);
    delay(10);
  }
  Serial.print("Observed LEFT min: "); Serial.println(leftMin);

  // 3) RIGHT: hold right, collect max
  Serial.println("Calibration: Look RIGHT and hold (2s)");
  delay(500);
  int rightMax = 0;
  for (int i = 0; i < 120; i++) {
    int v = filteredRead(EOG_PIN);
    if (v > 500 && v < 3500) rightMax = max(rightMax, v);
    delay(10);
  }
  Serial.print("Observed RIGHT max: "); Serial.println(rightMax);

  // Sanity fallback if extremes look wrong
  if (leftMin >= baseline) leftMin = baseline - 150;
  if (rightMax <= baseline) rightMax = baseline + 150;

  // Polarity handling: if both extremes on same side of baseline, enable invert
  if ((leftMin < baseline && rightMax < baseline) ||
      (leftMin > baseline && rightMax > baseline)) {
    invertPolarity = true;
    Serial.println("Note: Polarity appears biased. invertPolarity = true");
  }

  // Thresholds as safe midpoints with margin
  const int margin = 30; // raise if too sensitive, lower if too strict
  if (!invertPolarity) {
    leftThr  = (baseline + leftMin) / 2 + margin;  // slightly above leftMin
    rightThr = (baseline + rightMax) / 2 - margin; // slightly below rightMax
  } else {
    // If inverted, swap logic assumption
    leftThr  = (baseline + rightMax) / 2 - margin;
    rightThr = (baseline + leftMin) / 2 + margin;
  }

  Serial.print("LeftThr: ");  Serial.println(leftThr);
  Serial.print("RightThr: "); Serial.println(rightThr);
  Serial.println("Calibration complete. Enter password (LEFT/RIGHT only).");
}

/* ========== SETUP ========== */
void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("Starting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println();
  Serial.print("WiFi connected, IP: "); Serial.println(WiFi.localIP());

  server.on("/", [](){ server.send(200, "text/html", MAIN_page); });
  server.on("/status", [](){ server.send(200, "text/plain", authStatus); });
  server.on("/log", [](){ server.send(200, "text/plain", gestureLog); });
  server.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  analogReadResolution(12);

  calibrate();
}

/* ========== LOOP ========== */
void loop() {
  server.handleClient();

  int val = filteredRead(EOG_PIN);
  if (val < 500 || val > 3500) return;

  // Optional slow baseline drift correction within deadband
  if (abs(val - baseline) < deadband) {
    baseline = (0.995 * baseline) + (0.005 * val);
  }

  // Refractory period to avoid multiple detections from one movement
  if (millis() - lastGestureMs < refractoryMs) {
    return;
  }

  int gesture = 0;
  // Decide using thresholds with hysteresis: require crossing beyond thresholds
  if (val <= leftThr) {
    gesture = -1; // LEFT
  } else if (val >= rightThr) {
    gesture = 1;  // RIGHT
  }

  if (gesture != 0) {
    lastGestureMs = millis();
    inputSeq[indexSeq++] = gesture;

    if (gesture == 1) { Serial.println("👉 Gesture Detected: RIGHT"); gestureLog += "RIGHT "; }
    else { Serial.println("👈 Gesture Detected: LEFT"); gestureLog += "LEFT "; }

    Serial.print("Sequence so far: ");
    for (int i = 0; i < indexSeq; i++) Serial.print(inputSeq[i] == 1 ? "RIGHT " : "LEFT ");
    Serial.println();
  }

  if (indexSeq == 3) {
    bool match = true;
    for (int i = 0; i < 3; i++) if (inputSeq[i] != storedPassword[i]) { match = false; break; }

    if (match) {
      authStatus = "ACCESS GRANTED";
      digitalWrite(LED_PIN, HIGH);
      Serial.println("✅ ACCESS GRANTED");
    } else {
      authStatus = "ACCESS DENIED";
      digitalWrite(LED_PIN, LOW);
      Serial.println("❌ ACCESS DENIED");
    }
    accessTime = millis();
    indexSeq = 0;
    gestureLog = "";
  }

  // Reset status after 3s so frontend catches it
  if (authStatus != "Waiting" && millis() - accessTime > 3000) {
    authStatus = "Waiting";
    Serial.println("Enter Eye Password Again");
  }

  // Debug
  Serial.print("Val: "); Serial.print(val);
  Serial.print(" | Baseline: "); Serial.print(baseline);
  Serial.print(" | LeftThr: "); Serial.print(leftThr);
  Serial.print(" | RightThr: "); Serial.println(rightThr);
}
