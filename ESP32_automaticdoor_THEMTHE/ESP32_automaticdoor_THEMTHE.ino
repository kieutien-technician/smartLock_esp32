#include <WiFi.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

// ===== WiFi and Firebase =====
const char* ssid = "Tien";
const char* password_WIFI = "tien12345";
const char* firebaseBaseURL = "https://automatic-door-using-rfid-default-rtdb.firebaseio.com";

#define RX_PIN 17
#define TX_PIN 16

// RFID
#define SS_PIN 5
#define RST_PIN 4

// ===== RFID Configuration =====
#define RST_PIN 4
#define SS_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);

// ===== Keypad Configuration =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== Fingerprint Sensor =====
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
String inputID = "";

// ===== LCD Configuration =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define LED_PIN 15
#define BUZZER_PIN 2

// ===== System State Variables =====
bool doorState = false;
bool rfid_signal_new = false;
bool rfid_signal_delete = false;
bool fingerprint_signal_new = false;
bool fingerprint_signal_delete = false;

String smartlock_mastercard_id = "  366FB2C";

char lastKey = NO_KEY;
unsigned long keyPressTime = 0;
const unsigned long holdTime = 2000;

// ===== Password Settings =====
String password = "6666";
String passwordInput = "";
bool changePasswordMode = false;
bool confirmOldPassword = false;
String newPassword = "";
bool check = false;

// ===== Password Verification =====
void checkPassword() {
  lcd.clear();
  lcd.print("Enter Password:");
  passwordInput = "";
  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        if (passwordInput == password) {
          lcd.clear();
          lcd.print("Right Password");
          check = true;
          delay(1000);
          break;
        } else {
          lcd.clear();
          lcd.print("Wrong Password");
          check = false;
          delay(1000);
          break;
        }
      } else if (key != '*') {
        passwordInput += key;
        lcd.setCursor(0, 1);

        String maskedInput = "";
        for (int i = 0; i < passwordInput.length(); i++) {
          maskedInput += "*";
        }

        lcd.print(maskedInput);
      }
    }
  }
}

void setupLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Chao mung den");
  lcd.setCursor(0, 1);
  lcd.print("he thong khoa");
}

// ===== WiFi Connection =====
void connectWiFi() {
  WiFi.begin(ssid, password_WIFI);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected successfully");
}
bool lastDoorState = false;
bool lastBuzzerState = false;
// ===== Read Door and Buzzer States =====
void readFirebaseDoorState() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(firebaseBaseURL) + "/SmartLock/smartlock_state.json";
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        if (doc.containsKey("smartlock_state")) {
          JsonObject warning = doc["smartlock_state"];
          int state = doc["smartlock_state"];
          if (state == 1) {
            digitalWrite(LED_PIN, HIGH);
            doorState = true;
          } else if (state == 0) {
            digitalWrite(LED_PIN, LOW);
            doorState = false;
          }
        } else {
          Serial.println("JSON parse lỗi: " + String(error.c_str()));
        }

      } else {
        Serial.println("Lỗi GET Firebase: " + String(httpCode));
      }

      http.end();
    } else {
      Serial.println("WiFi chưa kết nối, không đọc được Firebase");
    }
  }
}
void readFirebaseBuzzer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(firebaseBaseURL) + "/SmartLock.json";

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        if (doc.containsKey("smartlock_warning")) {
          JsonObject warning = doc["smartlock_warning"];
          if (warning.containsKey("smartlock_warning_buzzer")) {
            int buzzer = warning["smartlock_warning_buzzer"];
            if (buzzer == 1) {
              digitalWrite(BUZZER_PIN, HIGH);
            } else {
              digitalWrite(BUZZER_PIN, LOW);
            }
          }
        }
      } else {
        Serial.println("JSON parse lỗi: " + String(error.c_str()));
      }
    } else {
      Serial.println("Lỗi GET Firebase: " + String(httpCode));
    }

    http.end();
  } else {
    Serial.println("WiFi chưa kết nối, không đọc được Firebase");
  }
}

// ===== Send Data to Firebase =====
void sendToFirebase(String path, String payload, bool update = false) {
  HTTPClient http;

  String firebaseURL = firebaseBaseURL + path + ".json?auth=your-firebase-auth-token";

  http.begin(firebaseURL);
  http.addHeader("Content-Type", "application/json");

  int httpCode;
  if (update) {
    httpCode = http.PATCH(payload);
  }

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.println("Error on sending request: " + String(httpCode));
  }
  http.end();
}

// ===== Unlock Door =====
void sendUnlock(String id, String method) {
  String payload;

  if (method == "rfid") {
    payload = "{\"rfid_signal\": {\"rfid_signal_smartlock\": 1}}";
    sendToFirebase("/SmartLock/RFID", payload, true);

    String idPayload = "{\"rfid_id_card_unlock\": \"" + id + "\"}";
    sendToFirebase("/SmartLock/RFID/rfid_id_card_handle", idPayload, true);
  } else if (method == "fingerprint") {
    payload = "{\"fingerprint_signal\": {\"fingerprint_signal_smartlock\": 1},";
    sendToFirebase("/SmartLock/FINGERPRINT", payload, true);
  }
  String statePayload = "{\"smartlock_state\": 1}";
  sendToFirebase("/SmartLock/smartlock_state", statePayload, true);
  digitalWrite(LED_PIN, HIGH);
  doorState = true;
}

// ===== Lock Door =====
void sendLock(String id = "") {
  String statePayload = "{\"smartlock_state\": 0}";

  sendToFirebase("/SmartLock/smartlock_state", statePayload, true);

  digitalWrite(LED_PIN, LOW);
  doorState = false;
}

// ===== RFID Authentication =====
void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  lcd.clear();
  lcd.print("RFID: ");
  lcd.print(uid);

  HTTPClient http;
  String url = String(firebaseBaseURL) + "/SmartLock/RFID/rfid_card_storage.json";
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      lcd.setCursor(0, 1);
      lcd.print("Parse JSON Err");
      Serial.println("JSON Error: " + String(error.c_str()));
    } else {
      bool matchFound = false;
      for (JsonPair kv : doc.as<JsonObject>()) {
        if (kv.value().as<String>() == uid) {
          matchFound = true;
          break;
        }
      }

      lcd.setCursor(0, 1);
      if (matchFound) {
        lcd.print("Access Granted");
        if (!doorState) sendUnlock(uid, "rfid");
        else sendLock();
        delay(1000);
        setupLCD();
      } else {
        lcd.print("Access Denied");
      }
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("HTTP Err: ");
    lcd.print(httpCode);
  }

  http.end();
  delay(1000);
  rfid.PICC_HaltA();
}

// ===== Add RFID Card =====
void addNewRFID() {
  rfid_signal_new = 1;
  lcd.clear();
  lcd.print("Quet the moi...");

  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    lcd.print(".");
    delay(500);
  }

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  lcd.clear();
  lcd.print("UID: ");
  lcd.print(uid);
  delay(2000);

  if (rfid_signal_new) {
    HTTPClient http;
    String url_uid = String(firebaseBaseURL) + "/SmartLock/RFID/rfid_id_card_handle.json";
    http.begin(url_uid);
    http.addHeader("Content-Type", "application/json");
    String payload_uid = "{\"rfid_id_card_new\":\"" + uid + "\"}";
    int code_uid = http.PATCH(payload_uid);
    if (code_uid == 200) {
      lcd.clear();
      lcd.print("RFID Added!");
    } else {
      lcd.clear();
      lcd.print("UID Err:");
      lcd.setCursor(0, 1);
      lcd.print(code_uid);
    }
    http.end();
    delay(500);

    String url_signal = String(firebaseBaseURL) + "/SmartLock/RFID/rfid_signal.json";
    http.begin(url_signal);
    http.addHeader("Content-Type", "application/json");
    int code_signal = http.PATCH("{\"rfid_signal_new\":1}");
    if (code_signal == 200) {
      lcd.clear();
      lcd.print("Signal Sent");
    } else {
      lcd.clear();
      lcd.print("Signal Err:");
      lcd.setCursor(0, 1);
      lcd.print(code_signal);
    }
    http.end();
    delay(500);

    http.begin(url_signal);
    http.addHeader("Content-Type", "application/json");
    http.PATCH("{\"rfid_signal_new\":0}");
    http.end();
    rfid_signal_new = false;
    lcd.clear();
    lcd.print("DA THEM THE");
    delay(2000);
  }
    setupLCD();
}

void deleteRFID() {
  rfid_signal_delete = 1;
  lcd.clear();
  lcd.print("Quet the xoa...");

  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    lcd.print(".");
    delay(500);
  }

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  lcd.clear();
  lcd.print("UID: ");
  lcd.print(uid);
  delay(2000);

  if (rfid_signal_delete) {
    HTTPClient http;

    String url_uid = String(firebaseBaseURL) + "/SmartLock/RFID/rfid_id_card_handle.json";
    http.begin(url_uid);
    http.addHeader("Content-Type", "application/json");
    String payload_uid = "{\"rfid_id_card_delete\":\"" + uid + "\"}";
    int code_uid = http.PATCH(payload_uid);
    if (code_uid == 200) {
      lcd.clear();
      lcd.print("RFID Deleted");
    } else {
      lcd.clear();
      lcd.print("Failed to Delete RFID");
    }
    http.end();
    delay(500);

    String url_signal = String(firebaseBaseURL) + "/SmartLock/RFID/rfid_signal.json";
    http.begin(url_signal);
    http.addHeader("Content-Type", "application/json");
    int code_signal = http.PATCH("{\"rfid_signal_delete\":1}");
    if (code_signal == 200) {
      lcd.clear();
      lcd.print("DEL Signal Sent");
    } else {
      lcd.clear();
      lcd.print("Failed to Send Signal");
    }
    http.end();
    delay(500);

    http.begin(url_signal);
    http.addHeader("Content-Type", "application/json");
    http.PATCH("{\"rfid_signal_delete\":0}");
    http.end();
    rfid_signal_delete = false;
    lcd.clear();
    lcd.print("DA XOA THE");
    delay(2000);
  }
  setupLCD();
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);

  finger.begin(57600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("He thong san sang");

  if (!finger.verifyPassword()) {
    lcd.setCursor(0, 1);
    lcd.print("Loi van tay!");
    while (1);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Cam bien OK");
  }

  delay(2000);
  lcd.clear();
  lcd.print("Ready!");
  delay(2000);
  setupLCD();
  connectWiFi();
}

unsigned long lastFirebaseUpdate = 0;
const unsigned long firebaseUpdateInterval = 4000;

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastFirebaseUpdate >= firebaseUpdateInterval) {
    readFirebaseDoorState();
    readFirebaseBuzzer();
    lastFirebaseUpdate = currentMillis;
  }
  char key = keypad.getKey();
  if (key) handleKeypadFingerprint(key);
  handleFingerprint();
  handleRFID();
}
void handleFingerprint()
{
  int fingerID = getFingerprintID();
  if (fingerID >= 0) {
    lcd.clear();
    lcd.print("Van tay OK ID:");
    lcd.setCursor(0, 1);
    lcd.print(fingerID);
    doorState = !doorState;
    digitalWrite(LED_PIN, doorState);
    delay(1000);
  } else if (fingerID == -2) {
    lcd.clear();
    lcd.print("Sai van tay!");
    delay(1000);
  }
  setupLCD();
}

void adNewFingerprint(char key)
{
    fingerprint_signal_new = true;
    inputID = "";
    if (fingerprint_signal_new) {
    if (key >= '0' && key <= '9') {
      inputID += key;
      lcd.setCursor(0, 1);
      lcd.print("Nhap ID: ");
      lcd.print(inputID);
    } else if (key == '#') {
      if (inputID.length() > 0) {
        int id = inputID.toInt();
        addFingerprint(id);
      } else {
        lcd.clear();
        lcd.print("ID khong hop le!");
      }
      inputID = "";
      fingerprint_signal_new = false;
    }

  }
  delay(1000);
  setupLCD();
}

void handleKeypadFingerprint(char key) {
  lcd.clear();
  lcd.print("Nhan phim: ");
  lcd.print(key);
  delay(300);
  if (key == '*') {
      sendLock();
      lcd.clear();
      lcd.print("Door Locked");
      delay(2000);
      setupLCD();
  }
  else if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
      checkPassword();
      if (check == true) {
        switch (key) {
          case 'A':
            addNewRFID();
            break;
          case 'B':
            deleteRFID();
            break;
          case 'C':
            lcd.clear();
            lcd.print("Nhap ID them #");
            inputID = "";
            adNewFingerprint(key);
            setupLCD();
            break;
        }
      } else {
        lcd.clear();
        lcd.print("Wrong Password");
        delay(2000);
      }
      setupLCD();
    }

}

int getFingerprintID() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerSearch() != FINGERPRINT_OK) return -2;
  return finger.fingerID;
}

void addFingerprint(int id) {
  lcd.clear();
  lcd.print("Dat tay len...");
  while (finger.getImage() != FINGERPRINT_OK);

  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Loi lan 1");
    return;
  }

  lcd.clear();
  lcd.print("Nhat tay roi");
  lcd.setCursor(0, 1);
  lcd.print("Dat lai...");
  delay(2000);

  while (finger.getImage() != FINGERPRINT_OK);

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Loi lan 2");
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Tao mau loi");
    return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Luu OK ID: ");
    lcd.print(id);
    int checkFingerID = getFingerprintID();
    if (checkFingerID == id) {
      lcd.clear();
      lcd.print("Van tay ID: ");
      lcd.print(id);
      lcd.setCursor(0, 1);
      lcd.print("Luu thanh cong");
    }
  } else {
    lcd.clear();
    lcd.print("Luu that bai");
  }
}

void adNewFingerprint(char key)
{
    if (fingerprint_signal_new) {
    if (key >= '0' && key <= '9') {
      inputID += key;
      lcd.setCursor(0, 1);
      lcd.print("Nhap ID: ");
      lcd.print(inputID);
    } else if (key == '#') {
      if (inputID.length() > 0) {
        int id = inputID.toInt();
        addFingerprint(id);
      } else {
        lcd.clear();
        lcd.print("ID khong hop le!");
      }
      inputID = "";
      fingerprint_signal_new = false;
    }

  }
  delay(1000);
  setupLCD();
}

void handleKeypadFingerprint(char key) {
  lcd.clear();
  lcd.print("Nhan phim: ");
  lcd.print(key);
  delay(300);
  if (key == '*') {
      sendLock();
      lcd.clear();
      lcd.print("Door Locked");
      delay(2000);
      setupLCD();
  }
  else if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
      checkPassword();
      if (check == true) {
        switch (key) {
          case 'A':
            addNewRFID();
            break;
          case 'B':
            deleteRFID();
            break;
          case 'C':
            lcd.clear();
            lcd.print("Nhap ID them #");
            fingerprint_signal_new = true;
            if(fingerprint_signal_new == true)
            {
              inputID = "";
              adNewFingerprint(key);
              setupLCD();
            }
            else break;
        }
      } else {
        lcd.clear();
        lcd.print("Wrong Password");
        delay(2000);
      }
      setupLCD();
    }

}
