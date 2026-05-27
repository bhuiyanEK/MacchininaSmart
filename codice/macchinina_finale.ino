#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>

// schermo
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SDA_PIN 10
#define SCL_PIN 9
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// sensore colore
#define S0 4
#define S1 5
#define S2 6
#define S3 7
#define OUT_PIN 15

// motori
#define IN1 12
#define IN2 13
#define ENA 14
#define IN3 20
#define IN4 21
#define ENB 47

// ultrasuoni 
#define TRIG_PIN  1
#define ECHO_PIN  2

//servo
#define SERVO_PIN 40
Servo radar;

// bottoni
#define BTN1 0   // rosso  (gara)
#define BTN2 8   // bianco (auto)
#define BTN3 42  // blu    (radiocomandato)

WebServer server(80);

SemaphoreHandle_t displayMutex;
TaskHandle_t taskTimerHandle = NULL;
TaskHandle_t taskWiFiHandle  = NULL;
TaskHandle_t taskMacchininaHandle = NULL;

// stato macchinina
enum MacchininaState { IDLE, READY, RUNNING, STOPPED, WIFI_MODE };
volatile MacchininaState macchininaState = STOPPED;
volatile int  gameMode        = 0;
volatile int  currentDuration = 30;
volatile unsigned long runStartTimestamp = 0;

volatile bool btn1Pressed = false;
volatile bool btn2Pressed = false;
volatile bool btn3Pressed = false;
unsigned long lastPress1 = 0, lastPress2 = 0, lastPress3 = 0;
const unsigned long debounceDelay = 200;

void IRAM_ATTR handleBtn1() { if (millis()-lastPress1 > debounceDelay) { btn1Pressed=true; lastPress1=millis(); } }
void IRAM_ATTR handleBtn2() { if (millis()-lastPress2 > debounceDelay) { btn2Pressed=true; lastPress2=millis(); } }
void IRAM_ATTR handleBtn3() { if (millis()-lastPress3 > debounceDelay) { btn3Pressed=true; lastPress3=millis(); } }

unsigned long redFreq, greenFreq, blueFreq;
int redVal, greenVal, blueVal;
unsigned long greenStartTime = 0;
bool greenDetected = false;

int speedForward = 70;
int speedTurn    = 85;
int stopDist     = 30;

const char* ssid = "MACCHININA";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>CONTROLLER</title>
  <style>
    body { background-color: #121212; color: white; font-family: sans-serif; text-align: center; overflow: hidden; }
    h2 { margin: 10px; }
    .container { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 80vh; }
    .row { display: flex; gap: 20px; margin: 10px; }
    .btn {
      width: 90px; height: 90px;
      background: #333; border: 2px solid #555; border-radius: 15px;
      font-size: 30px; color: white;
      display: flex; align-items: center; justify-content: center;
      user-select: none; -webkit-user-select: none; touch-action: manipulation;
    }
    .btn:active { background: #007bff; border-color: #0056b3; }
    .stop { background: #d32f2f; font-size: 16px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="row">
      <div class="btn" ontouchstart="mv('fwd')" ontouchend="stp()" onmousedown="mv('fwd')" onmouseup="stp()">&#8593;</div>
    </div>
    <div class="row">
      <div class="btn" ontouchstart="mv('lft')" ontouchend="stp()" onmousedown="mv('lft')" onmouseup="stp()">&#8592;</div>
      <div class="btn stop" onclick="stp()">STOP</div>
      <div class="btn" ontouchstart="mv('rgt')" ontouchend="stp()" onmousedown="mv('rgt')" onmouseup="stp()">&#8594;</div>
    </div>
    <div class="row">
      <div class="btn" ontouchstart="mv('bwd')" ontouchend="stp()" onmousedown="mv('bwd')" onmouseup="stp()">&#8595;</div>
    </div>
  </div>
  <script>
    function mv(dir) { fetch('/' + dir); }
    function stp() { fetch('/stop'); }
    document.addEventListener('contextmenu', event => event.preventDefault());
  </script>
</body>
</html>
)rawliteral";

void showBigTimer(int seconds) {
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(seconds >= 10 ? 40 : 55, 2);
  display.print(seconds);
  display.display();
}

void showText(String riga1, String riga2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(riga1);
  if (riga2 != "") {
    display.setCursor(0, 12);
    display.println(riga2);
  }
  display.display();
}

void countdownStart() {
  for (int i = 3; i > 0; i--) {
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
      showBigTimer(i);
      xSemaphoreGive(displayMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
    display.clearDisplay();
    display.display();
    xSemaphoreGive(displayMutex);
  }
}

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
void avanti() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, speedForward);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, speedForward);
}
void indietro() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, speedForward);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, speedForward);
}
void sinistra() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  analogWrite(ENA, speedTurn);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); analogWrite(ENB, speedTurn);
}
void destra() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); analogWrite(ENA, speedTurn);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  analogWrite(ENB, speedTurn);
}

void turnLeftTimed(int ms)  { sinistra(); vTaskDelay(pdMS_TO_TICKS(ms)); stopMotors(); }
void turnRightTimed(int ms) { destra();   vTaskDelay(pdMS_TO_TICKS(ms)); stopMotors(); }


int getDistance() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 25000);
  if (dur == 0) return 1;
  int d = dur * 0.0343 / 2;
  return (d > 400) ? 400 : d;
}


int checkDirection() {
  int distances[2] = {0, 0};
  radar.write(170); vTaskDelay(pdMS_TO_TICKS(400)); distances[0] = getDistance();
  radar.write(10);  vTaskDelay(pdMS_TO_TICKS(400)); distances[1] = getDistance();
  radar.write(90);  vTaskDelay(pdMS_TO_TICKS(300));
  if (distances[0] >= 200 && distances[1] >= 200) return 0;
  if (distances[0] <= stopDist && distances[1] <= stopDist) return 1;
  return (distances[0] >= distances[1]) ? 0 : 2;
}



String generatePassword(int len) {
  const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String p = "";
  for (int i = 0; i < len; i++) p += chars[random(0, sizeof(chars)-1)];
  return p;
}

void handleRoot()  { server.send(200, "text/html",  index_html); }
void handleFwd()   { avanti();     server.send(200, "text/plain", "OK"); }
void handleBwd()   { indietro();   server.send(200, "text/plain", "OK"); }
void handleLft()   { sinistra();   server.send(200, "text/plain", "OK"); }
void handleRgt()   { destra();     server.send(200, "text/plain", "OK"); }
void handleStop()  { stopMotors(); server.send(200, "text/plain", "OK"); }


//timer
void taskTimer(void* param) {
  while (true) {
    if (macchininaState == RUNNING) {
      unsigned long elapsed = (millis() - runStartTimestamp) / 1000;
      int rem = (int)currentDuration - (int)elapsed;

      if (rem <= 0) {
        stopMotors();
        macchininaState = STOPPED;
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) {
          showText("TEMPO", "SCADUTO!");
          xSemaphoreGive(displayMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
      } else {
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50))) {
          showBigTimer(rem);
          xSemaphoreGive(displayMutex);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}


//sito
void taskWiFi(void* param) {
  while (true) {
    if (macchininaState == WIFI_MODE) {
      server.handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


//funzione macchinina
void taskMacchinina(void* param) {
  while (true) {

    // bottoni
    if (btn1Pressed) {
      btn1Pressed = false;
      stopMotors(); WiFi.softAPdisconnect(true);
      gameMode = 1; currentDuration = 60;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) { showText("MODO GARA"); xSemaphoreGive(displayMutex); }
      vTaskDelay(pdMS_TO_TICKS(1000));
      countdownStart();
      greenDetected = false; greenStartTime = 0;
      macchininaState = IDLE;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) { showText("CERCO ROSSO..."); xSemaphoreGive(displayMutex); }
    }

    if (btn2Pressed) {
      btn2Pressed = false;
      stopMotors(); WiFi.softAPdisconnect(true);
      gameMode = 2; currentDuration = 30;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) { showText("MODO AUTO"); xSemaphoreGive(displayMutex); }
      vTaskDelay(pdMS_TO_TICKS(1000));
      countdownStart();
      runStartTimestamp = millis();
      macchininaState = RUNNING;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) { showText("VIA"); xSemaphoreGive(displayMutex); }
    }

    if (btn3Pressed) {
      btn3Pressed = false;
      stopMotors();
      gameMode = 3;
      macchininaState = WIFI_MODE;
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) { showText("AVVIO WIFI..."); xSemaphoreGive(displayMutex); }

      String wifiPassword = generatePassword(8);
      WiFi.softAP(ssid, wifiPassword.c_str());
      IPAddress myIP = WiFi.softAPIP();

      server.on("/",     handleRoot);
      server.on("/fwd",  handleFwd);
      server.on("/bwd",  handleBwd);
      server.on("/lft",  handleLft);
      server.on("/rgt",  handleRgt);
      server.on("/stop", handleStop);
      server.begin();

      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(200))) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);  display.println("RETE: " + String(ssid));
        display.setCursor(0, 10); display.println("PASS: " + wifiPassword);
        display.setCursor(0, 20); display.println("IP:   " + myIP.toString());
        display.display();
        xSemaphoreGive(displayMutex);
      }
    }

    if (macchininaState == WIFI_MODE || macchininaState == STOPPED) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // sensore colore (solo modo gara)
    bool isRed = false, isGreen = false, isBlue = false;
    if (gameMode == 1) {
      digitalWrite(S2, LOW);  digitalWrite(S3, LOW);  redFreq   = pulseIn(OUT_PIN, LOW, 50000);
      digitalWrite(S2, HIGH); digitalWrite(S3, HIGH); greenFreq = pulseIn(OUT_PIN, LOW, 50000);
      digitalWrite(S2, LOW);  digitalWrite(S3, HIGH); blueFreq  = pulseIn(OUT_PIN, LOW, 50000);
      redVal   = map(redFreq,   25,  400, 255, 0);
      greenVal = map(greenFreq, 30,  380, 255, 0);
      blueVal  = map(blueFreq,  20,  410, 255, 0);
      isRed   = (redVal   > 150 && redVal   > greenVal+30 && redVal   > blueVal +30);
      isGreen = (greenVal > 120 && greenVal >= redVal -30 && greenVal >= blueVal +10);
      isBlue  = (blueVal  > 140 && blueVal  > redVal +30 && blueVal  > greenVal+30);
    }

    // macchina a stati: modo gara
    if (gameMode == 1) {
      if (macchininaState == IDLE && isRed) {
        macchininaState = READY;
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) { showText("CERCO VERDE..."); xSemaphoreGive(displayMutex); }
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
      if (macchininaState == READY) {
        if (isGreen) {
          if (greenStartTime == 0) greenStartTime = millis();
          if (millis() - greenStartTime >= 1000) {
            macchininaState = RUNNING;
            runStartTimestamp = millis();
            greenDetected = true;
          }
        } else {
          greenStartTime = 0;
        }
      }
    }

    // movimento
    if (macchininaState == RUNNING) {
      if (gameMode == 1 && isBlue) {
        stopMotors();
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) { showText("BLU - STOP"); xSemaphoreGive(displayMutex); }
        macchininaState = STOPPED;
        vTaskDelay(pdMS_TO_TICKS(3000));
        continue;
      }

      radar.write(90);
      int distance = getDistance();

      if (distance >= stopDist) {
        avanti();
      } else {
        stopMotors();
        int turnDir = checkDirection();
        switch (turnDir) {
          case 0: turnLeftTimed(400);  break;
          case 1: turnLeftTimed(900);  break;
          case 2: turnRightTimed(400); break;
        }
        radar.write(90);
        vTaskDelay(pdMS_TO_TICKS(200));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for (;;); }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("SISTEMA AVVIATO");
  display.display();
  delay(1000);

  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT); pinMode(S3, OUTPUT);
  pinMode(OUT_PIN, INPUT);
  digitalWrite(S0, HIGH); digitalWrite(S1, LOW);

  radar.attach(SERVO_PIN);
  radar.write(90);

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);

  pinMode(BTN1, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(BTN1), handleBtn1, FALLING);
  pinMode(BTN2, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(BTN2), handleBtn2, FALLING);
  pinMode(BTN3, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(BTN3), handleBtn3, FALLING);

  randomSeed(analogRead(0));

  displayMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(taskTimer, "TaskTimer", 2048, NULL, 1, &taskTimerHandle, 0);
  xTaskCreatePinnedToCore(taskWiFi,  "TaskWiFi",  4096, NULL, 1, &taskWiFiHandle,  0);
  xTaskCreatePinnedToCore(taskMacchinina, "taskMacchinina", 8192, NULL, 2, &taskMacchininaHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
