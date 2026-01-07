/*************** BLYNK *****************/
#define BLYNK_TEMPLATE_ID   "TMPLXXXXXXX"      
#define BLYNK_TEMPLATE_NAME "Smart home"
#define BLYNK_AUTH_TOKEN    "MO3wL7DvOmq-mIDOpPAXGHRWVbUVJwGV"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>

/*************** KẾT NỐI CẢM BIẾN *********/
#define DHTPIN 4
#define DHTTYPE DHT11

#define PIR_PIN 27
#define GAS_PIN 34
#define FIRE_PIN 35
#define LDR_PIN 32

#define SERVO_MAIN 13   // Servo chính
#define SERVO_EXTRA 19  // Servo phụ
#define BUZZER_PIN 14
#define LED_PIN 12

#define RELAY_FAN 26
#define RELAY_LIGHT 25

#define GAS_THRESHOLD 1800
#define SERVO_OPEN_TIME 5000UL
#define ALERT_DISPLAY_TIME 5000UL
#define ALERT_INTERVAL 15000UL
#define ALERT_MAX_COUNT 3

/*************** WIFI *********/
char ssid[] = "Dong";       
char pass[] = "33333333";   

/*************** LOGIC RELAY *********/
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

/*************** ĐỐI TƯỢNG *********/
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Servo doorServoMain;
Servo doorServoExtra;

/*************** BIẾN *********/
bool servoIsOpen = false;
unsigned long servoCloseTimer = 0;

unsigned long lastAlertTime = 0;
int alertCount = 0;
unsigned long alertResetTimer = 0;
bool alertActive = false;

/*************** HÀM *********/
void doorOpen()  { 
  doorServoMain.write(90); 
  doorServoExtra.write(90);  // servo phụ mở cùng servo chính khi tự động
}
void doorClose() { 
  doorServoMain.write(0); 
  doorServoExtra.write(0);  // servo phụ đóng cùng servo chính khi tự động
}

void alarmOn()  { digitalWrite(BUZZER_PIN, HIGH); }
void alarmOff() { digitalWrite(BUZZER_PIN, LOW); }

/*************** BLYNK SWITCH *********/
BLYNK_WRITE(V1) { // Quạt
  digitalWrite(RELAY_FAN, param.asInt() ? RELAY_ON : RELAY_OFF);
}

BLYNK_WRITE(V2) { // Đèn
  digitalWrite(RELAY_LIGHT, param.asInt() ? RELAY_ON : RELAY_OFF);
}

BLYNK_WRITE(V3) { // Servo chính
  if (param.asInt()) doorServoMain.write(90);
  else doorServoMain.write(0);
}

BLYNK_WRITE(V7) { // Servo phụ (CuaPhu)
  if (param.asInt()) doorServoExtra.write(90);
  else doorServoExtra.write(0);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(FIRE_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);

  digitalWrite(RELAY_FAN, RELAY_OFF);
  digitalWrite(RELAY_LIGHT, RELAY_OFF);

  doorServoMain.attach(SERVO_MAIN);
  doorServoExtra.attach(SERVO_EXTRA);
  doorClose();

  dht.begin();
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.print("SMART ROOM");
  delay(2000);
  lcd.clear();

  // Kết nối WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Kết nối Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();

  unsigned long now = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  int gasValue = analogRead(GAS_PIN);
  bool gasDetected  = gasValue > GAS_THRESHOLD;
  bool fireDetected = digitalRead(FIRE_PIN) == LOW;

  bool motion = digitalRead(PIR_PIN);
  bool dark   = analogRead(LDR_PIN) < 2000;

  /* ===== GAS / LỬA ===== */
  if (gasDetected || fireDetected) {
    if (!servoIsOpen) {
      doorOpen();
      servoIsOpen = true;
      servoCloseTimer = now;
    }

    alarmOn();
    digitalWrite(LED_PIN, HIGH);

    digitalWrite(RELAY_FAN, gasDetected ? RELAY_ON : RELAY_OFF);

    lcd.clear();
    lcd.print(gasDetected ? "CANH BAO GAS!" : "PHAT HIEN LUA!");
    lcd.setCursor(0, 1);
    lcd.print(gasValue);
    lcd.print(" MO CUA");

  } else {
    if (servoIsOpen && (now - servoCloseTimer >= SERVO_OPEN_TIME)) {
      doorClose();
      servoIsOpen = false;
      alarmOff();
      digitalWrite(LED_PIN, LOW);
      digitalWrite(RELAY_FAN, RELAY_OFF);
    }
  }

  /* ===== CHUYỂN ĐỘNG / TỐI ===== */
  if ((motion || dark) && !alertActive) {
    if (now - alertResetTimer >= 60000UL) {
      alertCount = 0;
      alertResetTimer = now;
    }

    if (alertCount < ALERT_MAX_COUNT && (now - lastAlertTime >= ALERT_INTERVAL)) {
      alertActive = true;
      lastAlertTime = now;
      alertCount++;

      digitalWrite(RELAY_LIGHT, RELAY_ON); // bật đèn
      lcd.clear();
      lcd.print(motion ? "CO CHUYEN DONG" : "TROI TOI");
      lcd.setCursor(0, 1);
      lcd.print("BAT DEN");
    }
  }

  if (alertActive && (now - lastAlertTime >= ALERT_DISPLAY_TIME)) {
    alertActive = false;
    digitalWrite(RELAY_LIGHT, RELAY_OFF); // tắt đèn
    lcd.clear();
  }

  /* ===== HIỂN THỊ NHIỆT ĐỘ / ĐỘ ẨM ===== */
  if (!servoIsOpen && !alertActive && !isnan(t) && !isnan(h)) {
    lcd.setCursor(0, 0);
    lcd.print("Nhiet do:");
    lcd.print(t);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Do am:");
    lcd.print(h);
    lcd.print("%");

    // Gửi dữ liệu lên Blynk
    Blynk.virtualWrite(V5, t);
    Blynk.virtualWrite(V6, h);
  }

  delay(100);
}
