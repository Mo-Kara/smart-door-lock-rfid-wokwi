#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>

// ====== Wi-Fi (Wokwi defaults) ======
const char* ssid = "Wokwi-GUEST";
const char* wifiPassword = "";

// ====== MQTT Broker (public) ======
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ====== Topics ======
const char* statusTopic = "smartlock/status";
const char* logTopic = "smartlock/logs";
const char* commandTopic = "smartlock/command";

// ====== OLED Config ======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====== RFID RC522 Config ======
#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);
String authorizedUID = "DE AD BE EF"; // CHANGE THIS to your card's UID

// ====== Hardware ======
#define SERVO_PIN 16

Servo servo;
bool isLocked = true;
String correctPIN = "1234";
String enteredPIN = "";

// ====== Timer Variables ======
unsigned long unlockedStartTime = 0;
bool doorTimerActive = false;
const unsigned long AUTO_LOCK_DELAY = 5000;

// ====== Keypad Config ======
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; // Remapped away from SPI
byte colPins[COLS] = {14, 13, 15, 4};  // Remapped away from SPI
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== Helper Functions ======
void displayMessage(String msg, int delayTime = 0) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
  if (delayTime > 0) delay(delayTime);
}

void displayEnterPrompt() {
  displayMessage("Scan Card or\nEnter PIN:");
}

void logEvent(String msg) {
  Serial.println("LOG: " + msg);
  client.publish(logTopic, msg.c_str());
}

// ====== Lock/Unlock Logic ======
void lockDoor() {
  servo.write(90);
  isLocked = true;
  doorTimerActive = false;
  displayMessage("Door Locked", 1000);
  displayEnterPrompt();
  client.publish(statusTopic, "locked");
  logEvent("Door locked");
}

void unlockDoor() {
  servo.write(0);
  isLocked = false;
  displayMessage("Door Open\nAuto-locking soon...", 0);
  client.publish(statusTopic, "unlocked");
  logEvent("Door unlocked");

  unlockedStartTime = millis();
  doorTimerActive = true;
}

// ====== MQTT Callback ======
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());

  if (String(topic) == commandTopic) {
    if (message == "unlock") {
      unlockDoor();
    } else if (message == "lock") {
      lockDoor();
    } else {
      logEvent("Unknown MQTT command");
    }
  }
}

// ====== Connection Functions ======
void connectWiFi() {
  WiFi.begin(ssid, wifiPassword);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" Connected!");
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "WokwiSmartLock-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected");
      client.subscribe(commandTopic);
    } else {
      Serial.print("Failed: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  
  // Init I2C & SPI
  Wire.begin();
  SPI.begin();
  
  // Init Display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  displayEnterPrompt();

  // Init RFID
  rfid.PCD_Init();
  Serial.println("RFID Reader Ready");

  // Init Hardware
  servo.setPeriodHertz(50);             // Standard 50Hz servo frequency
  servo.attach(SERVO_PIN, 500, 2400);   // Pin, min pulse (us), max pulse (us)
  servo.write(0);  

  connectWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

// ====== Main Loop ======
void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  // --- NON-BLOCKING AUTO-LOCK TIMER ---
  if (doorTimerActive) {
    if (millis() - unlockedStartTime >= AUTO_LOCK_DELAY) {
      logEvent("Auto-lock after timeout");
      lockDoor();
    }
  }

  // --- RFID LOGIC ---
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uidString = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uidString += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      uidString += String(rfid.uid.uidByte[i], HEX);
    }
    uidString.trim();
    uidString.toUpperCase();
    
    Serial.println("Scanned UID: " + uidString);

    if (uidString == authorizedUID) {
      logEvent("RFID Authorized");
      unlockDoor();
    } else {
      displayMessage("Access Denied", 2000);
      displayEnterPrompt();
      logEvent("Invalid RFID: " + uidString);
    }
    
    // Halt PICC to stop reading the same card multiple times instantly
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // --- KEYPAD LOGIC ---
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      if (enteredPIN == correctPIN) {
        logEvent("PIN Authorized");
        unlockDoor();
      } else {
        displayMessage("Wrong PIN", 2000);
        displayEnterPrompt();
        logEvent("Wrong PIN entered");
      }
      enteredPIN = "";
    } else if (key == '*') {
      if (enteredPIN.length() == 0 && !isLocked) {
         lockDoor();
      } else {
         enteredPIN = "";
         displayEnterPrompt();
      }
    } else {
      enteredPIN += key;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("PIN: ");
      for (int i = 0; i < enteredPIN.length(); i++) display.print("*");
      display.display();
    }
  }
}