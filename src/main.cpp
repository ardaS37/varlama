#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522Extended.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <esp_system.h>

// Donanım baglantilari: RC522 ve SD ayni SPI hatti uzerindedir.
constexpr uint8_t RFID_SS = 5;
constexpr uint8_t RFID_RST = 27;
constexpr uint8_t SD_CS = 33;
constexpr uint8_t SD_SCK = 14;
constexpr uint8_t SD_MISO = 25;
constexpr uint8_t SD_MOSI = 26;
constexpr uint8_t LCD_SDA = 21;
constexpr uint8_t LCD_SCL = 22;
constexpr uint8_t LED_READY = 4;
constexpr uint8_t LED_SUCCESS = 16;
constexpr uint8_t LED_ERROR = 17;
constexpr uint8_t LED_INTERNET = 13;
constexpr uint8_t BUZZER_PIN = 32;
constexpr unsigned long CARD_DEBOUNCE_MS = 700;

// Kurulumdan once bunlari kendi ag bilgilerinize gore degistirin.
const char* DEFAULT_WIFI_SSID = "Turknet2";
const char* DEFAULT_WIFI_PASSWORD = "ardasa53272";
// Set a strong local password before deploying the access point.
const char* AP_PASSWORD = "CHANGE_THIS_PASSWORD";
const char* WEB_USERNAME = "admin";
const char* DEFAULT_WEB_PASSWORD = "Yoklama@826!";
const char* TZ_INFO = "<+03>-3"; // Turkiye sabit UTC+3

// MFRC522Extended, klasik RFID kartlara ek olarak ISO-DEP/APDU kullanan
// Android HCE telefonlarla da haberleşmek için kullanılır.
MFRC522Extended rfid(RFID_SS, RFID_RST);
LiquidCrystal_I2C lcd(0x3F, 16, 2);
AsyncWebServer server(80);
SPIClass sdSpi(HSPI);

struct Student {
  String number;
  String name;
  String school;
  String grade;
  String section;
  String room;
  bool permitted = false;
};

const char* STUDENTS_FILE = "/ogrenciler.csv";
const char* CARDS_FILE = "/kartlar.csv";
const char* EVENTS_FILE = "/giris_cikis.csv";
const char* CONFIG_FILE = "/ayarlar.txt";
const char* LEAVES_FILE = "/izinler.csv";
const char* GUESTS_FILE = "/misafirler.csv";
const char* CREDENTIALS_FILE = "/kimlikler.csv";
const byte CARD_KEY_BYTES[6] = {0x59, 0x4F, 0x4B, 0x4C, 0x41, 0x31}; // YOKLA1

// Android HCE uygulamasının seçmesi gereken özel uygulama kimliği (AID).
// Telefon, bu AID seçildikten sonra "YOK:<ogrenci_no>\x90\x00" döndürür.
const byte PHONE_AID[] = {0xF0, 0x59, 0x4F, 0x4B, 0x4C, 0x41}; // F0YOKLA

String lastUid;
unsigned long lastScanAt = 0;
String lastMessage = "Sistem hazir";
String lastStudent = "";
String lastError = "";
bool sdReady = false;
bool enrollmentMode = false;
bool adminEnrollmentMode = false;
bool adminEnrollmentUseUid = false;
bool guestEnrollmentMode = false;
bool credentialEnrollmentMode = false;
String credentialEnrollmentNumber;
String credentialEnrollmentName;
unsigned long lastPhysicalAdminAt = 0;
bool resetAdminScanPending = false;
bool resetAdminConfirmed = false;
unsigned long resetAdminConfirmedAt = 0;
File photoUploadFile;
String photoUploadNumber;
bool photoUploadFailed = false;
constexpr size_t MAX_PHOTO_BYTES = 200 * 1024;
String pendingUid;
String adminUid;
String adminCardToken;
bool systemUnlocked = false;
bool bootLocked = true;
bool lcdAnimationEnabled = true;
bool timeLockEnabled = false;
String earliestExitTime = "06:00";
String lastEntryTime = "22:00";
bool lateTrackingEnabled = false;
bool lateCardBlocked = false;
String webPassword = DEFAULT_WEB_PASSWORD;
String wifiSsid = DEFAULT_WIFI_SSID;
String wifiPassword = DEFAULT_WIFI_PASSWORD;
bool restartScheduled = false;
unsigned long restartAt = 0;
unsigned long lastWebActivity = 0;
bool internetAvailable = false;
unsigned long lastInternetCheck = 0;
unsigned long lcdMessageStarted = 0;
unsigned long lastWelcomeFrame = 0;
uint8_t welcomeFrame = 0;
bool welcomeScreenDrawn = false;

// SD kart takili olmasa bile temel kontrol panelinin acilmasi icin yerlesik sayfa.
#include "web_page.h"
/*
const char INDEX_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="tr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Pansiyon Yoklama</title>
<style>body{font:16px system-ui;margin:auto;max-width:680px;padding:24px;background:#f5f7fb;color:#182235}.card{background:#fff;border-radius:14px;padding:18px;margin:14px 0;box-shadow:0 2px 10px #18223512}.ok{font-weight:700;color:#146c43}label{display:block;margin-top:10px}input{width:100%;box-sizing:border-box;padding:9px;margin-top:4px;border:1px solid #c8d0dc;border-radius:7px}button,a{display:inline-block;margin-top:12px;padding:10px 14px;border:0;border-radius:7px;background:#155eef;color:#fff;text-decoration:none;font-weight:600}</style>
<body><header><h1>Pansiyon Kontrol</h1><small id="clock">Bağlanıyor...</small></header><section class="card hero"><div><b id="message">Sistem hazır</b><div id="detail"></div></div><div class="counts"><span><strong id="inside">0</strong> İçeride</span><span><strong id="outside">0</strong> Dışarıda</span></div></section><section class="card"><h2>Öğrenci ve kart tanımla</h2><p class="hint">Aynı öğrenci numarasını kullanarak birden fazla kart ekleyebilirsin.</p><button id="capture" type="button">Kart okut</button><span id="cardState"></span><form id="student"><label>Kart UID<input id="uid" name="uid" readonly required></label><div class="grid"><label>Öğrenci no<input name="number" required></label><label>Ad soyad<input name="name" required></label><label>Okul<input name="school" required></label><label>Sınıf<input name="grade" required></label><label>Şube<input name="section" required></label><label>Oda<input name="room"></label></div><button>Öğrenciyi / kartı kaydet</button></form><div id="result"></div></section><section class="card"><div class="title"><h2>Anlık durum</h2><input id="filter" placeholder="Ad, no, sınıf ara"></div><div id="list"></div></section><section class="card"><a href="/api/events" download="giris-cikis.csv">Giriş-çıkış kaydını indir</a></section><style>body{font:16px system-ui;margin:auto;max-width:960px;padding:24px;background:#f3f6fb;color:#172033}header{display:flex;justify-content:space-between;align-items:baseline}h1{margin:0}.card{background:#fff;border-radius:16px;padding:20px;margin:16px 0;box-shadow:0 3px 16px #17203312}.hero{display:flex;justify-content:space-between;align-items:center}.counts{display:flex;gap:16px}.counts span{background:#eef4ff;padding:10px 16px;border-radius:12px;text-align:center}.counts strong{display:block;font-size:24px;color:#155eef}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:0 14px}label{display:block;margin-top:11px}input{box-sizing:border-box;width:100%;padding:10px;border:1px solid #c8d0dc;border-radius:8px;margin-top:4px}button,a{display:inline-block;margin-top:14px;padding:10px 14px;border:0;border-radius:8px;background:#155eef;color:#fff;text-decoration:none;font-weight:650}.hint,small{color:#607086}.title{display:flex;justify-content:space-between;align-items:center}.title h2{margin:0}.title input{width:220px}.row{display:flex;justify-content:space-between;gap:10px;padding:12px 0;border-bottom:1px solid #edf0f5}.row:last-child{border:0}.meta{font-size:13px;color:#607086}.badge{padding:5px 9px;border-radius:99px;font-size:13px;font-weight:700}.in{background:#dcfae6;color:#087443}.out{background:#fee4e2;color:#b42318}@media(max-width:600px){.grid{grid-template-columns:1fr}.hero,.title{align-items:flex-start;gap:10px;flex-direction:column}.counts{width:100%}.counts span{flex:1}}</style><script>let rows=[];async function load(){try{let s=await fetch('/api/status').then(r=>r.json());message.textContent=s.message;detail.textContent=s.student?'Son işlem: '+s.student:'';clock.textContent=s.date+' · '+s.ip+' · SD: '+(s.sdReady?'hazır':'hata');let d=await fetch('/api/dashboard').then(r=>r.json());rows=d.students;inside.textContent=d.inside;outside.textContent=d.outside;render()}catch(e){message.textContent='Sunucuya ulaşılamıyor'}}function render(){let q=filter.value.toLocaleLowerCase('tr');list.innerHTML=rows.filter(x=>(x.name+x.number+x.grade+x.section+x.school).toLocaleLowerCase('tr').includes(q)).map(x=>`<div class="row"><div><b>${x.name}</b><div class="meta">${x.number} · ${x.school} · ${x.grade}/${x.section} · Oda ${x.room||'-'}<br>Son işlem: ${x.updated||'-'}</div></div><span class="badge ${x.state==='IN'?'in':'out'}">${x.state==='IN'?'İçeride':'Dışarıda'}</span></div>`).join('')||'<small>Kayıtlı öğrenci yok.</small>'}async function card(){try{let c=await fetch('/api/card/pending').then(r=>r.json());cardState.textContent=c.waiting?' Kart okutulması bekleniyor...':'';if(c.uid){uid.value=c.uid;cardState.textContent=' Kart okundu.'}}catch(e){}}capture.onclick=async()=>{await fetch('/api/card/capture',{method:'POST'});uid.value='';cardState.textContent=' Kart okutulması bekleniyor...'};student.onsubmit=async e=>{e.preventDefault();let r=await fetch('/api/students',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(Object.fromEntries(new FormData(student)))});let d=await r.json();result.textContent=r.ok?'Kart eşleştirildi.':(d.error||'Kayıt başarısız.');if(r.ok){student.reset();uid.value='';load()}};filter.oninput=render;load();card();setInterval(()=>{load();card()},3000)</script></body></html>
)HTML"; */

String csvField(const String& value) {
  String cleaned = value;
  cleaned.replace(";", " ");
  cleaned.replace("\n", " ");
  cleaned.replace("\r", " ");
  return cleaned;
}

String photoPathFor(const String& number) {
  String safe;
  for (size_t i = 0; i < number.length(); ++i) {
    char c = number[i];
    safe += (isAlphaNumeric(c) || c == '_' || c == '-') ? c : '_';
  }
  return "/photos/" + safe + ".jpg";
}

String uidToString() {
  String uid;
  for (byte i = 0; i < rfid.uid.size; ++i) {
    if (rfid.uid.uidByte[i] < 0x10) uid += '0';
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

String nowText(const char* format) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return "--";
  char buffer[24];
  strftime(buffer, sizeof(buffer), format, &timeinfo);
  return String(buffer);
}

String currentDeviceIp() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

int minutesFromClock(const String& value) {
  if (value.length() != 5 || value[2] != ':') return -1;
  int hour = value.substring(0, 2).toInt(), minute = value.substring(3, 5).toInt();
  return (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) ? hour * 60 + minute : -1;
}

bool attendanceTimeLocked() {
  if (!timeLockEnabled) return false;
  int start = minutesFromClock(earliestExitTime), end = minutesFromClock(lastEntryTime);
  if (start < 0 || end < 0) return false;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return false; // Saat bilinmiyorsa sistemi kilitleme.
  int now = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  bool allowed = start <= end ? (now >= start && now <= end) : (now >= start || now <= end);
  return !allowed;
}

bool afterLastEntryTime() {
  if (minutesFromClock(lastEntryTime) < 0) return false;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return false;
  return timeinfo.tm_hour * 60 + timeinfo.tm_min > minutesFromClock(lastEntryTime);
}

bool requireWebLogin(AsyncWebServerRequest* request) {
  if (request->authenticate(WEB_USERNAME, webPassword.c_str())) {
    unsigned long now = millis();
    lastWebActivity = now;
    return true;
  }
  request->requestAuthentication();
  return false;
}

const uint8_t trChars[][8] = {
  {0b00110,0b01001,0b01000,0b01000,0b01000,0b01001,0b00110,0b00000}, // Ç
  {0b00000,0b00000,0b00110,0b01001,0b01000,0b01001,0b00110,0b00000}, // ç
  {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b10001,0b00000}, // Ğ
  {0b00000,0b01110,0b00001,0b01111,0b10001,0b10001,0b01111,0b00000}, // ğ
  {0b00100,0b00000,0b01110,0b00100,0b00100,0b00100,0b01110,0b00000}, // İ
  {0b00100,0b00000,0b00110,0b00100,0b00100,0b00100,0b01110,0b00000}, // ı
  {0b00100,0b01010,0b01000,0b00110,0b00001,0b01001,0b00110,0b00000}, // Ş
  {0b00000,0b00000,0b01110,0b01000,0b00110,0b00001,0b01110,0b00000}  // ş
};
const uint8_t walkerLeft[][8] = {
  {0b00100,0b01110,0b00100,0b01110,0b10100,0b00100,0b01010,0b10001},
  {0b00100,0b01110,0b00100,0b01110,0b00101,0b00100,0b01010,0b00001}
};

void loadTurkishGlyphs() {
  for (uint8_t i = 0; i < 8; i++) lcd.createChar(i, (uint8_t*)trChars[i]);
}

void printTurkish(const String& text, uint8_t maxChars = 16) {
  uint8_t shown = 0;
  for (uint16_t i = 0; i < text.length() && shown < maxChars; ++i) {
    uint8_t first = text[i];
    if (first < 0x80) { lcd.print((char)first); shown++; continue; }
    if (i + 1 >= text.length()) break;
    uint8_t second = text[++i];
    char fallback = '?';
    if (first == 0xC3 && second == 0x87) fallback = 'C';
    else if (first == 0xC3 && second == 0xA7) fallback = 'c';
    else if (first == 0xC4 && second == 0x9E) fallback = 'G';
    else if (first == 0xC4 && second == 0x9F) fallback = 'g';
    else if (first == 0xC4 && second == 0xB0) fallback = 'I';
    else if (first == 0xC4 && second == 0xB1) fallback = 'i';
    else if (first == 0xC5 && second == 0x9E) fallback = 'S';
    else if (first == 0xC5 && second == 0x9F) fallback = 's';
    else if (first == 0xC3 && second == 0x96) fallback = 'O';
    else if (first == 0xC3 && second == 0xB6) fallback = 'o';
    else if (first == 0xC3 && second == 0x9C) fallback = 'U';
    else if (first == 0xC3 && second == 0xBC) fallback = 'u';
    lcd.print(fallback);
    shown++;
  }
}

String skipTurkishChars(const String& text, uint8_t skip) {
  uint16_t i = 0;
  while (i < text.length() && skip > 0) {
    i += text[i] < 0x80 ? 1 : 2;
    skip--;
  }
  return text.substring(i);
}

void showLcd(const String& line1, const String& line2 = "") {
  loadTurkishGlyphs();
  lcd.clear();
  lcd.setCursor(0, 0); printTurkish(line1);
  lcd.setCursor(0, 1); printTurkish(line2);
  lcdMessageStarted = millis();
  welcomeFrame = 0;
  welcomeScreenDrawn = false;
}

void updateLcdWelcome() {
  if (lcdMessageStarted != 0 && millis() - lcdMessageStarted < 10000) return;
  if (millis() - lastWelcomeFrame < 360) return;
  lastWelcomeFrame = millis();
  if (!systemUnlocked) {
    if (!welcomeScreenDrawn) {
      loadTurkishGlyphs(); lcd.clear();
      bool adminMissing = adminUid.isEmpty() && adminCardToken.isEmpty();
      lcd.setCursor(0, 0); printTurkish(adminMissing ? "Admin kart tanimla" : "Sistem kilitli");
      lcd.setCursor(0, 1); printTurkish(adminMissing ? "Panelden ekleyin" : "Admin kart okut");
      welcomeScreenDrawn = true;
    }
    return;
  }
  if (!lcdAnimationEnabled) {
    if (!welcomeScreenDrawn) {
      loadTurkishGlyphs(); lcd.clear();
      lcd.setCursor(0, 0); printTurkish("Hos geldiniz");
      lcd.setCursor(0, 1); printTurkish("Kart okutunuz");
      welcomeScreenDrawn = true;
    }
    return;
  }
  if (!welcomeScreenDrawn) {
    lcd.createChar(0, (uint8_t*)walkerLeft[0]);
    lcd.createChar(1, (uint8_t*)walkerLeft[1]);
    lcd.clear();
    lcd.setCursor(0, 1); printTurkish("Kart okutunuz");
    lcd.setCursor(15, 1); lcd.print('.');
    welcomeScreenDrawn = true;
  }
  const String greeting = "Hoş geldiniz";
  uint8_t position = (welcomeFrame / 2) % 16;
  uint8_t visible = 15 - position;
  lcd.setCursor(0, 0); lcd.print("                ");
  lcd.setCursor(position, 0); lcd.write((uint8_t)(welcomeFrame % 2));
  if (visible > 0) { lcd.setCursor(position + 1, 0); printTurkish(greeting, visible); }
  if (visible < 12) { lcd.setCursor(0, 0); printTurkish(skipTurkishChars(greeting, visible), 12 - visible); }
  welcomeFrame++;
}

void signalSuccess() {
  digitalWrite(LED_SUCCESS, HIGH);
  tone(BUZZER_PIN, 1800, 120);
  delay(180);
  digitalWrite(LED_SUCCESS, LOW);
}

void signalEntry() {
  digitalWrite(LED_SUCCESS, HIGH);
  tone(BUZZER_PIN, 2100, 120);
  delay(150);
  tone(BUZZER_PIN, 1400, 120);
  delay(160);
  digitalWrite(LED_SUCCESS, LOW);
}

void signalExit() {
  digitalWrite(LED_ERROR, HIGH);
  tone(BUZZER_PIN, 1400, 120);
  delay(150);
  tone(BUZZER_PIN, 2100, 120);
  delay(160);
  digitalWrite(LED_ERROR, LOW);
}

void signalBoot() {
  // Açılışta giriş onay melodisi üç kez çalınır.
  for (uint8_t i = 0; i < 3; i++) {
    signalEntry();
    delay(120);
  }
}

void scanI2cDevices() {
  Serial.println("I2C taramasi:");
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) Serial.printf("I2C cihaz bulundu: 0x%02X\n", address);
  }
}

void signalError() {
  digitalWrite(LED_SUCCESS, HIGH);
  digitalWrite(LED_ERROR, HIGH);
  tone(BUZZER_PIN, 350, 180);
  delay(260);
  tone(BUZZER_PIN, 350, 180);
  delay(220);
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);
}

void updateWebPanelIndicator() {
  if (lastInternetCheck == 0 || millis() - lastInternetCheck >= 15000) {
    lastInternetCheck = millis();
    internetAvailable = false;
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      internetAvailable = client.connect("1.1.1.1", 53, 800);
      client.stop();
    }
  }
  if (!internetAvailable) {
    digitalWrite(LED_INTERNET, LOW);
    return;
  }
  // Internet varken sabit yanar; yetkili panel acikken yanip soner.
  if (lastWebActivity != 0 && millis() - lastWebActivity <= 35000) {
    digitalWrite(LED_INTERNET, (millis() / 400) % 2 == 0 ? HIGH : LOW);
  } else {
    digitalWrite(LED_INTERNET, HIGH);
  }
}

void updateSystemLed() {
  digitalWrite(LED_READY, systemUnlocked ? HIGH : ((millis() / 500) % 2 == 0 ? HIGH : LOW));
}

String fieldAt(const String& row, uint8_t wanted) {
  int start = 0;
  for (uint8_t i = 0; i < wanted; ++i) { start = row.indexOf(';', start); if (start < 0) return ""; start++; }
  int end = row.indexOf(';', start);
  String value = end < 0 ? row.substring(start) : row.substring(start, end);
  value.trim(); return value;
}

bool findStudentByNumber(const String& number, Student& student) {
  File file = SD.open(STUDENTS_FILE, FILE_READ);
  if (!file) return false;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (row.isEmpty() || row.startsWith("number;")) continue;
    if (fieldAt(row, 0) == number) {
      student.number = number; student.name = fieldAt(row, 1); student.school = fieldAt(row, 2);
      student.grade = fieldAt(row, 3); student.section = fieldAt(row, 4); student.room = fieldAt(row, 5);
      student.permitted = fieldAt(row, 6) == "1";
      file.close(); return true;
    }
  }
  file.close(); return false;
}

bool findStudentByNumberAndName(const String& number, const String& name, Student& student) {
  File file = SD.open(STUDENTS_FILE, FILE_READ);
  if (!file) return false;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (row.isEmpty() || row.startsWith("number;")) continue;
    String savedName = fieldAt(row, 1);
    if (fieldAt(row, 0) == number && savedName.equalsIgnoreCase(name)) {
      student.number = number; student.name = savedName; student.school = fieldAt(row, 2);
      student.grade = fieldAt(row, 3); student.section = fieldAt(row, 4); student.room = fieldAt(row, 5);
      student.permitted = fieldAt(row, 6) == "1";
      file.close(); return true;
    }
  }
  file.close(); return false;
}

bool findCardOwner(const String& uid, String& number) {
  File file = SD.open(CARDS_FILE, FILE_READ);
  if (!file) return false;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (row.isEmpty() || row.startsWith("uid;")) continue;
    String savedUid = fieldAt(row, 0); savedUid.toUpperCase();
    if (savedUid == uid) { number = fieldAt(row, 1); file.close(); return true; }
  }
  file.close(); return false;
}

String lastStateFor(const String& number, String& updated);

bool isValidPhoneStudentNumber(const String& value) {
  if (value.length() < 2 || value.length() > 32) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (!isAlphaNumeric(c) && c != '_' && c != '-') return false;
  }
  return true;
}

bool isValidPhoneStudentName(const String& value) {
  if (value.length() < 2 || value.length() > 48) return false;
  return value.indexOf(';') < 0 && value.indexOf('|') < 0 && value.indexOf('\n') < 0 && value.indexOf('\r') < 0;
}

String phoneEventIdentity(const Student& student) {
  return student.number + "|" + student.name;
}

String lastStateForPhoneIdentity(const Student& student, String& updated) {
  String state = "OUT"; updated = "";
  File file = SD.open(EVENTS_FILE, FILE_READ);
  if (!file) return state;
  String identity = phoneEventIdentity(student);
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (!row.isEmpty() && !row.startsWith("date;") && fieldAt(row, 3) == identity) {
      state = fieldAt(row, 4); updated = fieldAt(row, 0) + " " + fieldAt(row, 1);
    }
  }
  file.close(); return state;
}

String lastStateForStudent(const Student& student, String& updated) {
  String phoneUpdated;
  String phoneState = lastStateForPhoneIdentity(student, phoneUpdated);
  if (!phoneUpdated.isEmpty()) { updated = phoneUpdated; return phoneState; }
  return lastStateFor(student.number, updated);
}

// Android HCE kartları her okutuluşta farklı UID verebilir. Bu nedenle UID
// bulunamazsa ISO-DEP üzerinden özel AID'yi seçip öğrenci numarasını isteriz.
// Bu ilk sürüm, hazır "NFC Card Emulator" ile uyumluluk testi içindir;
// üretimde imzalı challenge-response kullanılmalıdır.
bool readPhoneStudentIdentity(String& number, String& name) {
  // Bazı Android telefonlarda MFRC522 kütüphanesi SAK'taki ISO-DEP bitini
  // aktarmayabiliyor. Kayıtsız her UID için APDU denemesi yaparız; klasik
  // kart cevap vermezse aşağıda normal "kayıtsız kart" yoluna düşer.
  Serial.printf("Kayıtsız UID için HCE deneniyor: %s (SAK %02X)\n", uidToString().c_str(), rfid.uid.sak);

  byte selectApdu[5 + sizeof(PHONE_AID) + 1] = {0x00, 0xA4, 0x04, 0x00, sizeof(PHONE_AID)};
  memcpy(&selectApdu[5], PHONE_AID, sizeof(PHONE_AID));
  selectApdu[sizeof(selectApdu) - 1] = 0x00; // Le

  // MFRC522Extended PICC seçilirken RATS göndermiştir. Bazı HCE telefonlar
  // CID kabul etmez; bu sebeple APDU I-Block'u CID olmadan gönderiyoruz.
  rfid.tag.blockNumber = false;
  rfid.tag.ats.tc1.supportsCID = false;
  rfid.tag.ats.tc1.supportsNAD = false;
  byte response[64];
  byte responseLength = sizeof(response);
  MFRC522::StatusCode status = rfid.TCL_Transceive(&rfid.tag, selectApdu, sizeof(selectApdu), response, &responseLength);
  if (status != MFRC522::STATUS_OK) {
    Serial.printf("HCE APDU hatasi: %s\n", rfid.GetStatusCodeName(status));
    return false;
  }
  if (responseLength < 6 || response[responseLength - 2] != 0x90 || response[responseLength - 1] != 0x00) {
    Serial.println("HCE APDU gecersiz durum kelimesi");
    return false;
  }

  String payload;
  for (byte i = 0; i < responseLength - 2; ++i) payload += static_cast<char>(response[i]);
  if (!payload.startsWith("YOK:")) return false;
  String candidate = payload.substring(4);
  int separator = candidate.indexOf('|');
  if (separator >= 0) {
    name = candidate.substring(separator + 1); name.trim();
    candidate = candidate.substring(0, separator);
  } else name = ""; // Önceki telefon uygulamasıyla bir kez uyumluluk için.
  candidate.trim();
  if (!isValidPhoneStudentNumber(candidate) || (!name.isEmpty() && !isValidPhoneStudentName(name))) return false;
  number = candidate;
  Serial.printf("HCE telefon kimligi alindi: %s / %s\n", number.c_str(), name.c_str());
  return true;
}

String newCredentialToken() {
  char token[13];
  for (uint8_t i = 0; i < 6; ++i) sprintf(&token[i * 2], "%02X", (uint8_t)(esp_random() & 0xFF));
  token[12] = '\0';
  return String(token);
}

MFRC522::MIFARE_Key mifareKey(const byte* bytes) {
  MFRC522::MIFARE_Key key;
  memcpy(key.keyByte, bytes, 6);
  return key;
}

bool readMifareCredential(String& token) {
  MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
  if (type != MFRC522::PICC_TYPE_MIFARE_1K && type != MFRC522::PICC_TYPE_MIFARE_4K) return false;
  MFRC522::MIFARE_Key key = mifareKey(CARD_KEY_BYTES);
  if (rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &rfid.uid) != MFRC522::STATUS_OK) return false;
  byte block[18]; byte size = sizeof(block);
  if (rfid.MIFARE_Read(4, block, &size) != MFRC522::STATUS_OK || memcmp(block, "YKL1", 4) != 0) return false;
  char raw[13]; memcpy(raw, &block[4], 12); raw[12] = '\0';
  String value(raw);
  if (value.length() != 12) return false;
  for (uint8_t i = 0; i < value.length(); ++i) if (!isxdigit(value[i])) return false;
  token = value;
  return true;
}

bool writeMifareCredential(const String& token) {
  MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
  Serial.printf("Kart yazma: %s (SAK %02X)\n", rfid.PICC_GetTypeName(type), rfid.uid.sak);
  if (type != MFRC522::PICC_TYPE_MIFARE_1K && type != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println("Kart yazma: MIFARE Classic 1K/4K degil");
    return false;
  }
  if (token.length() != 12) { Serial.println("Kart yazma: token gecersiz"); return false; }
  const byte FACTORY_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  MFRC522::MIFARE_Key factory = mifareKey(FACTORY_KEY);
  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &factory, &rfid.uid);
  if (status != MFRC522::STATUS_OK) { Serial.printf("Kart yazma: blok 4 anahtar dogrulama hatasi: %s\n", rfid.GetStatusCodeName(status)); return false; }
  byte data[16] = {'Y','K','L','1'};
  memcpy(&data[4], token.c_str(), 12);
  status = rfid.MIFARE_Write(4, data, 16);
  if (status != MFRC522::STATUS_OK) { Serial.printf("Kart yazma: blok 4 yazma hatasi: %s\n", rfid.GetStatusCodeName(status)); return false; }
  byte trailer[16];
  memcpy(trailer, CARD_KEY_BYTES, 6);
  trailer[6] = 0xFF; trailer[7] = 0x07; trailer[8] = 0x80; trailer[9] = 0x69;
  memcpy(&trailer[10], CARD_KEY_BYTES, 6);
  status = rfid.MIFARE_Write(7, trailer, 16);
  if (status != MFRC522::STATUS_OK) Serial.printf("Kart yazma: guvenlik blogu yazma hatasi: %s\n", rfid.GetStatusCodeName(status));
  else Serial.println("Kart yazma: basarili");
  return status == MFRC522::STATUS_OK;
}

bool findCredentialOwner(const String& token, Student& student) {
  File file = SD.open(CREDENTIALS_FILE, FILE_READ);
  if (!file) return false;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (!row.isEmpty() && !row.startsWith("token;") && fieldAt(row, 0) == token) {
      bool found = findStudentByNumberAndName(fieldAt(row, 1), fieldAt(row, 2), student);
      file.close(); return found;
    }
  }
  file.close(); return false;
}

bool saveCredentialOwner(const String& token, const Student& student, const String& kind) {
  File file = SD.open(CREDENTIALS_FILE, FILE_APPEND);
  if (!file) return false;
  file.printf("%s;%s;%s;%s\n", token.c_str(), csvField(student.number).c_str(), csvField(student.name).c_str(), kind.c_str());
  file.close(); return true;
}

bool phoneIsoDepCommand(const byte* command, byte commandLength, byte* response, byte& responseLength, bool startSession = false) {
  if (startSession) {
    rfid.tag.blockNumber = false;
    rfid.tag.ats.tc1.supportsCID = false;
    rfid.tag.ats.tc1.supportsNAD = false;
  }
  return rfid.TCL_Transceive(&rfid.tag, const_cast<byte*>(command), commandLength, response, &responseLength) == MFRC522::STATUS_OK;
}

bool selectPhoneApplication(byte* response, byte& responseLength) {
  byte selectApdu[5 + sizeof(PHONE_AID) + 1] = {0x00, 0xA4, 0x04, 0x00, sizeof(PHONE_AID)};
  memcpy(&selectApdu[5], PHONE_AID, sizeof(PHONE_AID));
  selectApdu[sizeof(selectApdu) - 1] = 0x00;
  if (!phoneIsoDepCommand(selectApdu, sizeof(selectApdu), response, responseLength, true)) return false;
  return responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00;
}

bool readPhoneCredential(String& token) {
  byte response[64]; byte responseLength = sizeof(response);
  if (!selectPhoneApplication(response, responseLength) || responseLength < 8) return false;
  String payload;
  for (byte i = 0; i < responseLength - 2; ++i) payload += static_cast<char>(response[i]);
  if (!payload.startsWith("YOK:T:")) return false;
  String value = payload.substring(6); value.trim();
  if (value.length() != 12) return false;
  for (uint8_t i = 0; i < value.length(); ++i) if (!isxdigit(value[i])) return false;
  token = value;
  return true;
}

bool phoneReadyForProvisioning() {
  byte response[64]; byte responseLength = sizeof(response);
  if (!selectPhoneApplication(response, responseLength) || responseLength < 2) return false;
  String payload;
  for (byte i = 0; i < responseLength - 2; ++i) payload += static_cast<char>(response[i]);
  return payload == "YOK:READY";
}

bool provisionPhoneCredential(const String& token, const Student& student) {
  byte response[64]; byte responseLength = sizeof(response);
  if (!selectPhoneApplication(response, responseLength)) return false;
  String payload = token + "|" + student.number + "|" + student.name;
  if (payload.length() > 54) return false;
  byte command[64] = {0x80, 0x10, 0x00, 0x00, (byte)payload.length()};
  memcpy(&command[5], payload.c_str(), payload.length());
  responseLength = sizeof(response);
  if (!phoneIsoDepCommand(command, payload.length() + 5, response, responseLength)) return false;
  return responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00;
}

String lastStateFor(const String& number, String& updated) {
  String state = "OUT"; updated = "";
  File file = SD.open(EVENTS_FILE, FILE_READ);
  if (!file) return state;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (row.isEmpty() || row.startsWith("date;")) continue;
    if (fieldAt(row, 3) == number) { state = fieldAt(row, 4); updated = fieldAt(row, 0) + " " + fieldAt(row, 1); }
  }
  file.close(); return state;
}

bool hasActiveLeave(const String& number, String* until = nullptr) {
  File file = SD.open(LEAVES_FILE, FILE_READ); if (!file) return false;
  String now = nowText("%Y-%m-%dT%H:%M"); bool active = false;
  while (file.available()) { String row = file.readStringUntil('\n'); row.trim(); if (!row.isEmpty() && !row.startsWith("number;") && fieldAt(row, 0) == number && now >= fieldAt(row, 1) && now <= fieldAt(row, 2)) { active = true; if (until) *until = fieldAt(row, 2); } }
  file.close(); return active;
}

bool appendAttendanceEvent(const String& uid, const String& number, const String& state, bool late = false, bool manual = false, const String& reason = "") {
  String date = nowText("%Y-%m-%d"), time = nowText("%H:%M:%S");
  File file = SD.open(EVENTS_FILE, FILE_APPEND); if (!file) return false;
  file.printf("%s;%s;%s;%s;%s;%d;%d;%s\n", date.c_str(), time.c_str(), csvField(uid).c_str(), csvField(number).c_str(), state.c_str(), late ? 1 : 0, manual ? 1 : 0, csvField(reason).c_str()); file.close();
  if (!SD.exists("/yedekler")) SD.mkdir("/yedekler"); String path = "/yedekler/" + date + ".csv"; bool isNew = !SD.exists(path); File backup = SD.open(path, FILE_APPEND);
  if (backup) { if (isNew) backup.println("date;time;uid;student_no;event;late;manual;reason"); backup.printf("%s;%s;%s;%s;%s;%d;%d;%s\n", date.c_str(), time.c_str(), csvField(uid).c_str(), csvField(number).c_str(), state.c_str(), late ? 1 : 0, manual ? 1 : 0, csvField(reason).c_str()); backup.close(); }
  return true;
}

void loadSettings() {
  File file = SD.open(CONFIG_FILE, FILE_READ);
  if (!file) return;
  adminUid = file.readStringUntil('\n');
  adminUid.trim(); adminUid.toUpperCase();
  while (file.available()) {
    String line = file.readStringUntil('\n'); line.trim();
    if (line == "boot_locked=0") bootLocked = false;
    if (line == "animation=0") lcdAnimationEnabled = false;
    if (line == "time_lock=1") timeLockEnabled = true;
    if (line == "late_tracking=1") lateTrackingEnabled = true;
    if (line == "late_block=1") lateCardBlocked = true;
    if (line.startsWith("earliest_exit=")) earliestExitTime = line.substring(14);
    if (line.startsWith("last_entry=")) lastEntryTime = line.substring(11);
    if (line.startsWith("web_password=")) webPassword = line.substring(13);
    if (line.startsWith("wifi_ssid=")) wifiSsid = line.substring(10);
    if (line.startsWith("wifi_password=")) wifiPassword = line.substring(14);
    if (line.startsWith("admin_card_token=")) adminCardToken = line.substring(17);
  }
  file.close();
}

bool saveSettings() {
  if (SD.exists(CONFIG_FILE)) SD.remove(CONFIG_FILE);
  File file = SD.open(CONFIG_FILE, FILE_WRITE);
  if (!file) return false;
  file.println(adminUid);
  file.printf("boot_locked=%d\n", bootLocked ? 1 : 0);
  file.printf("animation=%d\n", lcdAnimationEnabled ? 1 : 0);
  file.printf("time_lock=%d\n", timeLockEnabled ? 1 : 0);
  file.printf("late_tracking=%d\n", lateTrackingEnabled ? 1 : 0);
  file.printf("late_block=%d\n", lateCardBlocked ? 1 : 0);
  file.print("earliest_exit="); file.println(earliestExitTime);
  file.print("last_entry="); file.println(lastEntryTime);
  file.print("web_password="); file.println(webPassword);
  file.print("wifi_ssid="); file.println(wifiSsid);
  file.print("wifi_password="); file.println(wifiPassword);
  file.print("admin_card_token="); file.println(adminCardToken);
  file.close();
  return true;
}

bool saveAdminUid(const String& uid) { adminUid = uid; adminCardToken = ""; return saveSettings(); }
bool saveAdminCardToken(const String& token) { adminCardToken = token; adminUid = ""; return saveSettings(); }

void recordCard() {
  String uid = uidToString();
  Serial.print("RFID kart UID: ");
  Serial.println(uid);
  if (uid == lastUid && millis() - lastScanAt < CARD_DEBOUNCE_MS) return;
  lastUid = uid; lastScanAt = millis();
  if (adminEnrollmentMode) {
    adminEnrollmentMode = false;
    if (adminEnrollmentUseUid) {
      if (saveAdminUid(uid)) {
        lastMessage = "Admin UID kaydedildi"; lastStudent = "Admin kart";
        showLcd("Admin UID kayitli", "Sistem kilitli"); signalSuccess();
      } else { showLcd("SD yazma hatasi"); signalError(); }
      return;
    }
    String token = newCredentialToken();
    if (writeMifareCredential(token) && saveAdminCardToken(token)) {
      lastMessage = "Admin kart yazildi"; lastStudent = "Admin kart";
      showLcd("Admin kart yazildi", "Sistem kilitli"); signalSuccess();
    } else { showLcd("Admin kart yazma", "Kart bos olmali"); signalError(); }
    return;
  }
  if (resetAdminScanPending) {
    resetAdminScanPending = false;
    String resetToken;
    bool resetAdminCard = !adminCardToken.isEmpty() && readMifareCredential(resetToken) && resetToken == adminCardToken;
    if (resetAdminCard || (!adminUid.isEmpty() && uid == adminUid)) {
      resetAdminConfirmed = true;
      resetAdminConfirmedAt = millis();
      lastMessage = "Sifirlama admin kart onayi"; lastStudent = "Admin kart";
      showLcd("Admin onaylandi", "Panelden sifirla"); signalSuccess();
    } else {
      resetAdminConfirmed = false;
      resetAdminConfirmedAt = 0;
      lastMessage = "Sifirlama admin kart hatasi";
      showLcd("Yanlis admin kart", "Sifirlama iptal"); signalError();
    }
    return;
  }
  String scannedAdminToken;
  bool isAdminCard = !adminCardToken.isEmpty() && readMifareCredential(scannedAdminToken) && scannedAdminToken == adminCardToken;
  if (isAdminCard || (!adminUid.isEmpty() && uid == adminUid)) {
    systemUnlocked = !systemUnlocked;
    lastPhysicalAdminAt = millis();
    lastMessage = systemUnlocked ? "Yoklama acildi" : "Yoklama kilitlendi";
    lastStudent = "Admin kart";
    showLcd(systemUnlocked ? "Sistem acik" : "Sistem kilitli", currentDeviceIp());
    if (systemUnlocked) signalEntry(); else signalExit();
    return;
  }
  if (!systemUnlocked) {
    lastMessage = "Sistem kilitli"; lastStudent = "Admin kart bekleniyor";
    showLcd("Sistem kilitli", "Admin kart okut"); signalError();
    return;
  }
  if (enrollmentMode) {
    pendingUid = uid;
    enrollmentMode = false;
    lastMessage = "Kart kayda hazir";
    lastStudent = uid;
    showLcd("Kart alindi", "Bilgileri girin");
    signalSuccess();
    return;
  }
  if (credentialEnrollmentMode) {
    Student target;
    if (!findStudentByNumberAndName(credentialEnrollmentNumber, credentialEnrollmentName, target)) {
      credentialEnrollmentMode = false;
      showLcd("Ogrenci bulunamadi"); signalError(); return;
    }
    String token = newCredentialToken();
    String kind;
    bool written = writeMifareCredential(token);
    if (written) kind = "CARD";
    else if (provisionPhoneCredential(token, target)) { written = true; kind = "PHONE"; }
    // Bazı HCE uygulamaları telefona özel APDU ile yazmayı desteklemez;
    // zaten YOK:numara|ad kimliği yayınlıyorsa bunu eşleştirip doğrudan kullan.
    if (!written) {
      String phoneNumber, phoneName;
      if (readPhoneStudentIdentity(phoneNumber, phoneName) && phoneNumber == target.number && (phoneName.isEmpty() || phoneName == target.name)) {
        written = true; kind = "PHONE_DIRECT";
      }
    }
    if (!written || (kind != "PHONE_DIRECT" && !saveCredentialOwner(token, target, kind))) {
      lastMessage = "Kimlik yuklenemedi"; lastStudent = "";
      showLcd("Yukleme basarisiz", "Kart/telefon hazir mi?"); signalError(); return;
    }
    credentialEnrollmentMode = false;
    lastMessage = kind == "CARD" ? "Karta kimlik yazildi" : (kind == "PHONE_DIRECT" ? "Telefon dogrulandi" : "Telefona kimlik yazildi");
    lastStudent = target.name;
    showLcd(kind == "CARD" ? "Kart yuklendi" : (kind == "PHONE_DIRECT" ? "Telefon tanimli" : "Telefon yuklendi"), target.name);
    signalSuccess(); return;
  }
  if (attendanceTimeLocked()) {
    lastMessage = "Saat disi kart okutma kapali"; lastStudent = "";
    showLcd("Sistem saat kilitli", earliestExitTime + " - " + lastEntryTime); signalError();
    return;
  }
  Student student;
  String token;
  String number;
  String phoneName;
  bool phoneCredential = false;
  // Önce eski UID eşleşmesini dene. UID kayıtlı değilse kart/telefonun
  // içindeki korumalı kimliği oku; en son aşamada hata sesi ver.
  bool uidCredential = findCardOwner(uid, number);
  bool protectedCredential = false;
  bool studentFound = uidCredential && findStudentByNumber(number, student);
  if (!uidCredential) {
    protectedCredential = readMifareCredential(token) || readPhoneCredential(token);
    if (protectedCredential) studentFound = findCredentialOwner(token, student);
  }
  if (!uidCredential && !protectedCredential) {
    phoneCredential = readPhoneStudentIdentity(number, phoneName);
    studentFound = phoneCredential && !phoneName.isEmpty()
      ? findStudentByNumberAndName(number, phoneName, student)
      : findStudentByNumber(number, student);
  }
  if (!studentFound && phoneReadyForProvisioning()) {
    lastMessage = "Telefon yuklemeye hazir"; lastStudent = "";
    showLcd("Telefon hazir", "Panelden yukle"); signalSuccess(); return;
  }
  if (!studentFound) {
    lastMessage = protectedCredential ? "Kimlik kaydi bulunamadi" : (phoneCredential ? "Telefon ogrenci bulunamadi" : "Kayitsiz kart");
    showLcd(protectedCredential ? "Kimlik kaydi yok" : (phoneCredential ? "Telefon kaydi yok" : "Kayitsiz kart"), uid); signalError(); return;
  }
  String updated;
  String nextState = (protectedCredential || phoneCredential ? lastStateForPhoneIdentity(student, updated) : lastStateFor(number, updated)) == "OUT" ? "IN" : "OUT";
  bool lateEntry = nextState == "IN" && lateTrackingEnabled && afterLastEntryTime();
  if (lateEntry && lateCardBlocked) {
    lastMessage = "Son giris saati gecti"; lastStudent = student.name;
    showLcd("Son giris saati", "Kart islemi kapali"); signalError(); return;
  }
  String eventUid = protectedCredential ? "KIMLIK:" + token : (phoneCredential ? "TELEFON:" + uid : uid);
  String eventStudent = (protectedCredential || phoneCredential) ? phoneEventIdentity(student) : number;
  if (!appendAttendanceEvent(eventUid, eventStudent, nextState, lateEntry)) { showLcd("SD yazma hatasi"); signalError(); return; }
  lastMessage = lateEntry ? "Gec giris kaydedildi" : (nextState == "IN" ? "Pansiyona giris" : "Pansiyondan cikis");
  lastStudent = student.name;
  showLcd(nextState == "IN" ? "Giris yapildi" : "Cikis yapildi", student.name);
  if (nextState == "IN") signalEntry(); else signalExit();
}

void addStudent(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject data = json.as<JsonObject>();
  String uid = data["uid"] | ""; uid.trim(); uid.toUpperCase();
  String number = data["number"] | ""; number.trim();
  String name = data["name"] | ""; name.trim();
  String school = data["school"] | ""; school.trim();
  String grade = data["grade"] | ""; grade.trim();
  String section = data["section"] | ""; section.trim();
  String room = data["room"] | ""; room.trim();
  String permittedValue = data["permitted"] | "";
  bool permitted = permittedValue == "1" || permittedValue == "true" || permittedValue == "on";
  if (uid.isEmpty() || number.isEmpty() || name.isEmpty() || school.isEmpty() || grade.isEmpty() || section.isEmpty()) {
    request->send(400, "application/json", "{\"error\":\"Kart, öğrenci no, ad, okul, sınıf ve şube zorunludur.\"}"); return;
  }
  Student existing;
  bool studentExists = findStudentByNumberAndName(number, name, existing);
  String owner;
  if (findCardOwner(uid, owner)) {
    if (owner != number) { request->send(409, "application/json", "{\"error\":\"Kart başka öğrenciye tanımlı.\"}"); return; }
    // Eski/yarım kayıtta kart satırı kalıp öğrenci satırı eksik kalmış olabilir.
    if (studentExists) { request->send(409, "application/json", "{\"error\":\"Bu kart zaten bu öğrenciye tanımlı.\"}"); return; }
  }
  if (!studentExists) {
    File students = SD.open(STUDENTS_FILE, FILE_APPEND);
    if (!students) { request->send(500, "application/json", "{\"error\":\"SD kart yazılamadı.\"}"); return; }
    students.printf("%s;%s;%s;%s;%s;%s;%d\n", csvField(number).c_str(), csvField(name).c_str(), csvField(school).c_str(), csvField(grade).c_str(), csvField(section).c_str(), csvField(room).c_str(), permitted ? 1 : 0);
    students.close();
  }
  // Aynı karta ait eksik öğrenci satırını onardık; kartı bir kez daha yazma.
  if (!owner.isEmpty()) { request->send(201, "application/json", "{\"ok\":true,\"repaired\":true}"); return; }
  File cards = SD.open(CARDS_FILE, FILE_APPEND);
  if (!cards) { request->send(500, "application/json", "{\"error\":\"SD kart yazılamadı.\"}"); return; }
  cards.printf("%s;%s\n", uid.c_str(), csvField(number).c_str()); cards.close();
  request->send(201, "application/json", "{\"ok\":true}");
}

bool updateStudentPermission(const String& number, bool permitted) {
  const char* TEMP_FILE = "/ogrenciler.tmp";
  File input = SD.open(STUDENTS_FILE, FILE_READ);
  if (!input) return false;
  if (SD.exists(TEMP_FILE)) SD.remove(TEMP_FILE);
  File output = SD.open(TEMP_FILE, FILE_WRITE);
  if (!output) { input.close(); return false; }
  bool found = false;
  while (input.available()) {
    String row = input.readStringUntil('\n'); row.trim();
    if (row.isEmpty()) continue;
    if (!row.startsWith("number;") && fieldAt(row, 0) == number) {
      output.printf("%s;%s;%s;%s;%s;%s;%d\n", csvField(fieldAt(row, 0)).c_str(), csvField(fieldAt(row, 1)).c_str(), csvField(fieldAt(row, 2)).c_str(), csvField(fieldAt(row, 3)).c_str(), csvField(fieldAt(row, 4)).c_str(), csvField(fieldAt(row, 5)).c_str(), permitted ? 1 : 0);
      found = true;
    } else output.println(row);
  }
  input.close(); output.close();
  if (!found) { SD.remove(TEMP_FILE); return false; }
  SD.remove(STUDENTS_FILE);
  return SD.rename(TEMP_FILE, STUDENTS_FILE);
}

bool updateStudentDetails(const String& number, const String& name, const String& school, const String& grade, const String& section, const String& room) {
  const char* TEMP_FILE = "/ogrenciler.tmp";
  File input = SD.open(STUDENTS_FILE, FILE_READ);
  if (!input) return false;
  if (SD.exists(TEMP_FILE)) SD.remove(TEMP_FILE);
  File output = SD.open(TEMP_FILE, FILE_WRITE);
  if (!output) { input.close(); return false; }
  bool found = false;
  while (input.available()) {
    String row = input.readStringUntil('\n'); row.trim();
    if (row.isEmpty()) continue;
    if (!row.startsWith("number;") && fieldAt(row, 0) == number) {
      output.printf("%s;%s;%s;%s;%s;%s;%s\n", csvField(number).c_str(), csvField(name).c_str(), csvField(school).c_str(), csvField(grade).c_str(), csvField(section).c_str(), csvField(room).c_str(), fieldAt(row, 6).c_str());
      found = true;
    } else output.println(row);
  }
  input.close(); output.close();
  if (!found) { SD.remove(TEMP_FILE); return false; }
  SD.remove(STUDENTS_FILE);
  return SD.rename(TEMP_FILE, STUDENTS_FILE);
}

bool setStudentLeave(const String& number, const String& start, const String& end) {
  const char* temp = "/izinler.tmp";
  File input = SD.open(LEAVES_FILE, FILE_READ);
  if (!input) return false;
  if (SD.exists(temp)) SD.remove(temp);
  File output = SD.open(temp, FILE_WRITE);
  if (!output) { input.close(); return false; }
  output.println("number;start;end");
  while (input.available()) { String row = input.readStringUntil('\n'); row.trim(); if (!row.isEmpty() && !row.startsWith("number;") && fieldAt(row, 0) != number) output.println(row); }
  output.printf("%s;%s;%s\n", csvField(number).c_str(), start.c_str(), end.c_str());
  input.close(); output.close(); SD.remove(LEAVES_FILE); return SD.rename(temp, LEAVES_FILE);
}

bool removeCardUid(const String& uid) {
  const char* temp = "/kartlar.tmp";
  File input = SD.open(CARDS_FILE, FILE_READ); if (!input) return false;
  if (SD.exists(temp)) SD.remove(temp);
  File output = SD.open(temp, FILE_WRITE); if (!output) { input.close(); return false; }
  output.println("uid;number"); bool removed = false;
  while (input.available()) { String row = input.readStringUntil('\n'); row.trim(); if (!row.isEmpty() && !row.startsWith("uid;")) { if (fieldAt(row, 0) == uid) removed = true; else output.println(row); } }
  input.close(); output.close(); if (!removed) { SD.remove(temp); return false; }
  SD.remove(CARDS_FILE); return SD.rename(temp, CARDS_FILE);
}

bool resetAllData() {
  // Kayıtlar ve ayarlar silinir; yazılım/Wi-Fi ayarları flash'ta kaldığı için etkilenmez.
  if (SD.exists(STUDENTS_FILE)) SD.remove(STUDENTS_FILE);
  if (SD.exists(CARDS_FILE)) SD.remove(CARDS_FILE);
  if (SD.exists(EVENTS_FILE)) SD.remove(EVENTS_FILE);
  if (SD.exists(CONFIG_FILE)) SD.remove(CONFIG_FILE);
  const char* TEMP_FILE = "/ogrenciler.tmp";
  if (SD.exists(TEMP_FILE)) SD.remove(TEMP_FILE);
  File students = SD.open(STUDENTS_FILE, FILE_WRITE);
  File cards = SD.open(CARDS_FILE, FILE_WRITE);
  File events = SD.open(EVENTS_FILE, FILE_WRITE);
  if (!students || !cards || !events) { if (students) students.close(); if (cards) cards.close(); if (events) events.close(); return false; }
  students.println("number;name;school;grade;section;room;permitted");
  cards.println("uid;number");
  events.println("date;time;uid;student_no;event");
  students.close(); cards.close(); events.close();
  adminUid = ""; adminCardToken = ""; bootLocked = true; systemUnlocked = false; lcdAnimationEnabled = true;
  timeLockEnabled = false; earliestExitTime = "06:00"; lastEntryTime = "22:00"; lateTrackingEnabled = false; lateCardBlocked = false;
  webPassword = DEFAULT_WEB_PASSWORD;
  pendingUid = ""; enrollmentMode = false; adminEnrollmentMode = false;
  resetAdminScanPending = false; resetAdminConfirmed = false; resetAdminConfirmedAt = 0;
  lastMessage = "Tum kayitlar silindi"; lastStudent = "";
  showLcd("Kayitlar silindi", "Sistem kilitli");
  return true;
}

bool fileHasRows(const char* path, const char* headerStart) {
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  while (file.available()) {
    String row = file.readStringUntil('\n'); row.trim();
    if (!row.isEmpty() && !row.startsWith(headerStart)) { file.close(); return true; }
  }
  file.close(); return false;
}

// Önceki sürüm /students.csv ve /attendance.csv kullanıyordu.
// Dosyaları silmeden, yeni dosyalar boşsa kayıtları bir defa içe aktarır.
void migrateLegacyRecords() {
  if (!fileHasRows(STUDENTS_FILE, "number;") && SD.exists("/students.csv")) {
    File oldFile = SD.open("/students.csv", FILE_READ);
    File students = SD.open(STUDENTS_FILE, FILE_APPEND);
    File cards = SD.open(CARDS_FILE, FILE_APPEND);
    int imported = 0;
    while (oldFile && oldFile.available()) {
      String row = oldFile.readStringUntil('\n'); row.trim();
      if (row.isEmpty() || row.startsWith("uid;")) continue;
      String uid = fieldAt(row, 0); uid.toUpperCase();
      String number = fieldAt(row, 1), name = fieldAt(row, 2), room = fieldAt(row, 3);
      Student known; String owner;
      if (!number.isEmpty() && !findStudentByNumber(number, known)) {
        students.printf("%s;%s;Eski kayit;-;-;%s;0\n", csvField(number).c_str(), csvField(name).c_str(), csvField(room).c_str());
      }
      if (!uid.isEmpty() && !findCardOwner(uid, owner)) cards.printf("%s;%s\n", uid.c_str(), csvField(number).c_str());
      imported++;
    }
    if (oldFile) oldFile.close(); if (students) students.close(); if (cards) cards.close();
    Serial.printf("Eski ogrenci kayitlari aktarildi: %d\n", imported);
  }
  if (!fileHasRows(EVENTS_FILE, "date;") && SD.exists("/attendance.csv")) {
    File oldFile = SD.open("/attendance.csv", FILE_READ);
    File events = SD.open(EVENTS_FILE, FILE_APPEND);
    int imported = 0;
    while (oldFile && oldFile.available()) {
      String row = oldFile.readStringUntil('\n'); row.trim();
      if (row.isEmpty() || row.startsWith("date;")) continue;
      String number = fieldAt(row, 3), updated;
      String state = lastStateFor(number, updated) == "OUT" ? "IN" : "OUT";
      events.printf("%s;%s;%s;%s;%s\n", fieldAt(row, 0).c_str(), fieldAt(row, 2).c_str(), fieldAt(row, 1).c_str(), csvField(number).c_str(), state.c_str());
      imported++;
    }
    if (oldFile) oldFile.close(); if (events) events.close();
    Serial.printf("Eski gecmis aktarildi: %d\n", imported);
  }
}

void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html; charset=utf-8", INDEX_PAGE);
    response->addHeader("Cache-Control", "no-store, max-age=0");
    request->send(response);
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    JsonDocument doc;
    String deviceIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["sdReady"] = sdReady; doc["sdTotal"] = sdReady ? SD.totalBytes() : 0; doc["sdUsed"] = sdReady ? SD.usedBytes() : 0; doc["ip"] = deviceIp;
    doc["wifiSsid"] = wifiSsid;
    doc["date"] = nowText("%d.%m.%Y %H:%M"); doc["message"] = lastMessage; doc["student"] = lastStudent;
    doc["systemUnlocked"] = systemUnlocked; doc["adminRegistered"] = !adminCardToken.isEmpty() || !adminUid.isEmpty();
    doc["bootLocked"] = bootLocked; doc["animationEnabled"] = lcdAnimationEnabled;
    doc["timeLockEnabled"] = timeLockEnabled; doc["earliestExitTime"] = earliestExitTime; doc["lastEntryTime"] = lastEntryTime; doc["attendanceTimeLocked"] = attendanceTimeLocked();
    doc["lateTrackingEnabled"] = lateTrackingEnabled; doc["lateCardBlocked"] = lateCardBlocked;
    bool resetApprovalValid = resetAdminConfirmed && millis() - resetAdminConfirmedAt <= 60000;
    if (!resetApprovalValid) resetAdminConfirmed = false;
    doc["resetAdminPending"] = resetAdminScanPending; doc["resetAdminConfirmed"] = resetApprovalValid;
    String output; serializeJson(doc, output); request->send(200, "application/json", output);
  });
  server.on("/api/card/capture", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    enrollmentMode = true;
    pendingUid = "";
    lastUid = "";
    lastMessage = "Kayit icin kart bekleniyor";
    showLcd("Kayit modu", "Kart okutun");
    request->send(200, "application/json", "{\"ok\":true}");
  });
  AsyncCallbackJsonWebHandler* credentialCapture = new AsyncCallbackJsonWebHandler("/api/credential/capture", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String number = data["number"] | ""; number.trim();
    String name = data["name"] | ""; name.trim();
    Student student;
    if (!systemUnlocked || (adminCardToken.isEmpty() && adminUid.isEmpty())) {
      request->send(403, "application/json", "{\"error\":\"Önce fiziksel admin kartıyla sistemi açın.\"}"); return;
    }
    if (!findStudentByNumberAndName(number, name, student)) {
      request->send(404, "application/json", "{\"error\":\"Öğrenci numarası ve ad soyad eşleşmedi.\"}"); return;
    }
    credentialEnrollmentNumber = number; credentialEnrollmentName = student.name;
    credentialEnrollmentMode = true; enrollmentMode = false; pendingUid = ""; lastUid = "";
    lastMessage = "Kart veya telefon yukleme bekliyor";
    showLcd("Kimlik yukleme", "Kart/telefon okut");
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(credentialCapture);
  server.on("/api/admin/capture", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    adminEnrollmentMode = true;
    adminEnrollmentUseUid = request->hasParam("mode") && request->getParam("mode")->value() == "uid";
    enrollmentMode = false;
    pendingUid = ""; lastUid = "";
    lastMessage = adminEnrollmentUseUid ? "Admin UID bekleniyor" : "Admin ID kart bekleniyor";
    showLcd(adminEnrollmentUseUid ? "Admin UID kaydi" : "Admin ID kart kaydi", "Kart okutunuz");
    request->send(200, "application/json", "{\"ok\":true}");
  });
  AsyncCallbackJsonWebHandler* systemSettings = new AsyncCallbackJsonWebHandler("/api/system", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String action = data["action"] | "";
    if (action == "unlock" || action == "lock") {
      systemUnlocked = action == "unlock";
      lastMessage = systemUnlocked ? "Yoklama webden acildi" : "Yoklama webden kilitlendi";
      showLcd(systemUnlocked ? "Sistem acik" : "Sistem kilitli", currentDeviceIp());
    }
    if (!data["bootLocked"].isNull()) bootLocked = data["bootLocked"].as<bool>();
    if (!data["animationEnabled"].isNull()) lcdAnimationEnabled = data["animationEnabled"].as<bool>();
    if (!data["timeLockEnabled"].isNull()) timeLockEnabled = data["timeLockEnabled"].as<bool>();
    if (!data["lateTrackingEnabled"].isNull()) lateTrackingEnabled = data["lateTrackingEnabled"].as<bool>();
    if (!data["lateCardBlocked"].isNull()) lateCardBlocked = data["lateCardBlocked"].as<bool>();
    if (!data["earliestExitTime"].isNull()) { String value = data["earliestExitTime"] | ""; if (minutesFromClock(value) < 0) { request->send(400, "application/json", "{\"error\":\"En erken çıkış saati geçersiz.\"}"); return; } earliestExitTime = value; }
    if (!data["lastEntryTime"].isNull()) { String value = data["lastEntryTime"] | ""; if (minutesFromClock(value) < 0) { request->send(400, "application/json", "{\"error\":\"Son giriş saati geçersiz.\"}"); return; } lastEntryTime = value; }
    if (!saveSettings()) { request->send(500, "application/json", "{\"error\":\"Ayarlar SD karta yazilamadi.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(systemSettings);
  AsyncCallbackJsonWebHandler* passwordSettings = new AsyncCallbackJsonWebHandler("/api/password", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String current = data["current"] | "";
    String next = data["next"] | "";
    if (current != webPassword) { request->send(403, "application/json", "{\"error\":\"Mevcut parola hatalı.\"}"); return; }
    if (next.length() < 8) { request->send(400, "application/json", "{\"error\":\"Yeni parola en az 8 karakter olmalı.\"}"); return; }
    webPassword = next;
    if (!saveSettings()) { request->send(500, "application/json", "{\"error\":\"Parola SD karta yazılamadı.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(passwordSettings);
  AsyncCallbackJsonWebHandler* wifiSettings = new AsyncCallbackJsonWebHandler("/api/wifi", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String ssid = data["ssid"] | ""; ssid.trim();
    String password = data["password"] | "";
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 || (!password.isEmpty() && password.length() < 8)) {
      request->send(400, "application/json", "{\"error\":\"Ağ adı 1-32 karakter; parola boş veya 8-63 karakter olmalı.\"}"); return;
    }
    wifiSsid = ssid; wifiPassword = password;
    if (!saveSettings()) { request->send(500, "application/json", "{\"error\":\"Wi-Fi ayarları SD karta yazılamadı.\"}"); return; }
    restartScheduled = true; restartAt = millis() + 1200;
    request->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
  });
  server.addHandler(wifiSettings);
  server.on("/api/card/pending", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    JsonDocument doc;
    doc["waiting"] = enrollmentMode || adminEnrollmentMode || resetAdminScanPending;
    doc["uid"] = pendingUid;
    doc["target"] = adminEnrollmentMode ? "admin" : resetAdminScanPending ? "reset" : enrollmentMode ? "student" : "";
    String output; serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  server.on("/api/dashboard", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    JsonDocument doc;
    JsonArray list = doc["students"].to<JsonArray>();
    int inside = 0, outside = 0;
    File file = SD.open(STUDENTS_FILE, FILE_READ);
    if (file) {
      while (file.available()) {
        String row = file.readStringUntil('\n'); row.trim();
        if (row.isEmpty() || row.startsWith("number;")) continue;
        String number = fieldAt(row, 0), updated;
        Student current; current.number = number; current.name = fieldAt(row, 1);
        String state = lastStateForStudent(current, updated);
        JsonObject item = list.add<JsonObject>();
        item["number"] = number; item["name"] = fieldAt(row, 1); item["school"] = fieldAt(row, 2);
        bool permitted = fieldAt(row, 6) == "1" || hasActiveLeave(number);
        item["grade"] = fieldAt(row, 3); item["section"] = fieldAt(row, 4); item["room"] = fieldAt(row, 5); item["permitted"] = permitted;
        item["state"] = state; item["updated"] = updated;
        if (!permitted) { if (state == "IN") inside++; else outside++; }
      }
      file.close();
    }
    doc["inside"] = inside; doc["outside"] = outside;
    String output; serializeJson(doc, output); request->send(200, "application/json", output);
  });
  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    if (!SD.exists(EVENTS_FILE)) { request->send(200, "text/csv; charset=utf-8", "date;time;uid;student_no;event\n"); return; }
    request->send(SD, EVENTS_FILE, "text/csv; charset=utf-8");
  });
  server.on("/api/backups", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    if (!request->hasParam("date")) { request->send(400, "text/plain", "Tarih gerekli."); return; }
    String date = request->getParam("date")->value();
    if (date.length() != 10 || date.indexOf("..") >= 0) { request->send(400, "text/plain", "Tarih geçersiz."); return; }
    String path = "/yedekler/" + date + ".csv";
    if (!SD.exists(path)) { request->send(404, "text/plain", "Yedek bulunamadı."); return; }
    request->send(SD, path, "text/csv; charset=utf-8");
  });
  server.on("/api/rooms", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    const int MAX_ROOMS = 32; String names[MAX_ROOMS]; int ins[MAX_ROOMS] = {}, outs[MAX_ROOMS] = {}, count = 0;
    File file = SD.open(STUDENTS_FILE, FILE_READ);
    if (file) { while (file.available()) { String row = file.readStringUntil('\n'); row.trim(); if (row.isEmpty() || row.startsWith("number;")) continue; String room = fieldAt(row, 5); if (room.isEmpty()) room = "Belirtilmemiş"; int slot = -1; for (int i=0;i<count;i++) if(names[i]==room) slot=i; if(slot<0 && count<MAX_ROOMS){slot=count++;names[slot]=room;} if(slot>=0){String updated; Student current; current.number=fieldAt(row,0); current.name=fieldAt(row,1); if(lastStateForStudent(current,updated)=="IN")ins[slot]++;else outs[slot]++;} } file.close(); }
    JsonDocument doc; JsonArray rooms=doc["rooms"].to<JsonArray>(); for(int i=0;i<count;i++){JsonObject room=rooms.add<JsonObject>();room["room"]=names[i];room["inside"]=ins[i];room["outside"]=outs[i];} String output;serializeJson(doc,output);request->send(200,"application/json",output);
  });
  server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    JsonDocument doc; doc["sdReady"]=sdReady; doc["sdTotal"]=sdReady?SD.totalBytes():0; doc["sdUsed"]=sdReady?SD.usedBytes():0; doc["wifiConnected"]=WiFi.status()==WL_CONNECTED; doc["wifiRssi"]=WiFi.status()==WL_CONNECTED?WiFi.RSSI():0; doc["rfidFirmware"]=rfid.PCD_ReadRegister(MFRC522::VersionReg); doc["lcd"]=true; doc["lastMessage"]=lastMessage; String output;serializeJson(doc,output);request->send(200,"application/json",output);
  });
  server.on("/api/activity", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    const int MAX_EVENTS = 30;
    String recent[MAX_EVENTS]; int totalRows = 0;
    File file = SD.open(EVENTS_FILE, FILE_READ);
    if (file) {
      while (file.available()) {
        String row = file.readStringUntil('\n'); row.trim();
        if (!row.isEmpty() && !row.startsWith("date;")) { recent[totalRows % MAX_EVENTS] = row; totalRows++; }
      }
      file.close();
    }
    JsonDocument doc; JsonArray events = doc["events"].to<JsonArray>();
    int start = totalRows > MAX_EVENTS ? totalRows - MAX_EVENTS : 0;
    for (int i = totalRows - 1; i >= start; --i) {
      String row = recent[i % MAX_EVENTS], number = fieldAt(row, 3);
      Student student; findStudentByNumber(number, student);
      JsonObject event = events.add<JsonObject>();
      event["time"] = fieldAt(row, 0) + " " + fieldAt(row, 1);
      event["uid"] = fieldAt(row, 2); event["number"] = number;
      event["name"] = student.name; event["state"] = fieldAt(row, 4);
    }
    String output; serializeJson(doc, output); request->send(200, "application/json", output);
  });
  server.on("/api/late", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    const int MAX_LATE = 50;
    String recent[MAX_LATE]; int count = 0;
    File file = SD.open(EVENTS_FILE, FILE_READ);
    if (file) {
      while (file.available()) {
        String row = file.readStringUntil('\n'); row.trim();
        if (!row.isEmpty() && !row.startsWith("date;") && fieldAt(row, 5) == "1") { recent[count % MAX_LATE] = row; count++; }
      }
      file.close();
    }
    JsonDocument doc; doc["enabled"] = lateTrackingEnabled; doc["blocked"] = lateCardBlocked; doc["lastEntryTime"] = lastEntryTime;
    JsonArray events = doc["events"].to<JsonArray>();
    int start = count > MAX_LATE ? count - MAX_LATE : 0;
    for (int i = count - 1; i >= start; --i) {
      String row = recent[i % MAX_LATE], number = fieldAt(row, 3);
      Student student; findStudentByNumber(number, student);
      JsonObject event = events.add<JsonObject>();
      event["date"] = fieldAt(row, 0); event["time"] = fieldAt(row, 1); event["number"] = number; event["name"] = student.name; event["uid"] = fieldAt(row, 2);
    }
    String output; serializeJson(doc, output); request->send(200, "application/json", output);
  });
  server.on("/api/student-events", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    if (!request->hasParam("number")) { request->send(400, "application/json", "{\"error\":\"Öğrenci numarası gerekli.\"}"); return; }
    String number = request->getParam("number")->value();
    const int MAX_EVENTS = 60;
    String recent[MAX_EVENTS]; int totalRows = 0;
    File file = SD.open(EVENTS_FILE, FILE_READ);
    if (file) {
      while (file.available()) {
        String row = file.readStringUntil('\n'); row.trim();
        if (!row.isEmpty() && !row.startsWith("date;") && fieldAt(row, 3) == number) { recent[totalRows % MAX_EVENTS] = row; totalRows++; }
      }
      file.close();
    }
    Student student; findStudentByNumber(number, student);
    JsonDocument doc; doc["number"] = number; doc["name"] = student.name;
    doc["school"] = student.school; doc["grade"] = student.grade; doc["section"] = student.section; doc["room"] = student.room; doc["permitted"] = student.permitted;
    doc["photoAvailable"] = SD.exists(photoPathFor(number));
    String leaveUntil; doc["leaveActive"] = hasActiveLeave(number, &leaveUntil); doc["leaveUntil"] = leaveUntil;
    JsonArray cards = doc["cards"].to<JsonArray>();
    File cardFile = SD.open(CARDS_FILE, FILE_READ);
    if (cardFile) { while (cardFile.available()) { String cardRow = cardFile.readStringUntil('\n'); cardRow.trim(); if (!cardRow.isEmpty() && !cardRow.startsWith("uid;") && fieldAt(cardRow, 1) == number) cards.add(fieldAt(cardRow, 0)); } cardFile.close(); }
    JsonArray events = doc["events"].to<JsonArray>();
    int start = totalRows > MAX_EVENTS ? totalRows - MAX_EVENTS : 0;
    for (int i = totalRows - 1; i >= start; --i) {
      String row = recent[i % MAX_EVENTS]; JsonObject event = events.add<JsonObject>();
      event["date"] = fieldAt(row, 0); event["time"] = fieldAt(row, 1); event["uid"] = fieldAt(row, 2); event["state"] = fieldAt(row, 4);
    }
    String output; serializeJson(doc, output); request->send(200, "application/json", output);
  });
  AsyncCallbackJsonWebHandler* studentPermission = new AsyncCallbackJsonWebHandler("/api/student/permitted", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String number = data["number"] | ""; number.trim();
    if (number.isEmpty() || data["permitted"].isNull()) { request->send(400, "application/json", "{\"error\":\"Öğrenci ve izin bilgisi gerekli.\"}"); return; }
    if (!updateStudentPermission(number, data["permitted"].as<bool>())) { request->send(500, "application/json", "{\"error\":\"Öğrenci güncellenemedi.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(studentPermission);
  AsyncCallbackJsonWebHandler* leaveSettings = new AsyncCallbackJsonWebHandler("/api/student/leave", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>(); String number = data["number"] | "", start = data["start"] | "", end = data["end"] | ""; Student student;
    if (!findStudentByNumber(number, student) || start.length() != 16 || end.length() != 16 || start > end || !setStudentLeave(number, start, end)) { request->send(400, "application/json", "{\"error\":\"İzin bilgileri geçersiz veya kaydedilemedi.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(leaveSettings);
  AsyncCallbackJsonWebHandler* manualEvent = new AsyncCallbackJsonWebHandler("/api/manual-event", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>(); String number = data["number"] | "", state = data["state"] | "", reason = data["reason"] | ""; Student student;
    if (!findStudentByNumber(number, student) || (state != "IN" && state != "OUT") || reason.isEmpty() || !appendAttendanceEvent("MANUEL", number, state, false, true, reason)) { request->send(400, "application/json", "{\"error\":\"Manuel işlem kaydedilemedi.\"}"); return; }
    lastMessage = "Manuel islem"; lastStudent = student.name; request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(manualEvent);
  AsyncCallbackJsonWebHandler* removeCard = new AsyncCallbackJsonWebHandler("/api/card/remove", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>(); String uid = data["uid"] | "", password = data["password"] | "";
    if (password != webPassword || !removeCardUid(uid)) { request->send(400, "application/json", "{\"error\":\"Kart kaldırılamadı.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(removeCard);
  AsyncCallbackJsonWebHandler* studentDetails = new AsyncCallbackJsonWebHandler("/api/student/details", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String number = data["number"] | ""; number.trim();
    String name = data["name"] | ""; name.trim();
    String school = data["school"] | ""; school.trim();
    String grade = data["grade"] | ""; grade.trim();
    String section = data["section"] | ""; section.trim();
    String room = data["room"] | ""; room.trim();
    if (number.isEmpty() || name.isEmpty() || school.isEmpty() || grade.isEmpty() || section.isEmpty()) { request->send(400, "application/json", "{\"error\":\"Ad, okul, sınıf ve şube zorunludur.\"}"); return; }
    if (!updateStudentDetails(number, name, school, grade, section, room)) { request->send(500, "application/json", "{\"error\":\"Öğrenci bilgileri güncellenemedi.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(studentDetails);
  server.on("/api/student/photo", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    if (!request->hasParam("number")) { request->send(400, "text/plain", "Öğrenci numarası gerekli."); return; }
    String path = photoPathFor(request->getParam("number")->value());
    if (!SD.exists(path)) { request->send(404, "text/plain", "Fotoğraf yok."); return; }
    request->send(SD, path, "image/jpeg");
  });
  server.on("/api/student/photo", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      if (!requireWebLogin(request)) return;
      if (photoUploadFile) photoUploadFile.close();
      if (photoUploadFailed) { String path = photoPathFor(photoUploadNumber); if (SD.exists(path)) SD.remove(path); request->send(400, "application/json", "{\"error\":\"Fotoğraf en fazla 200 KB olmalı ve JPEG seçilmeli.\"}"); return; }
      request->send(200, "application/json", "{\"ok\":true}");
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!request->authenticate(WEB_USERNAME, webPassword.c_str())) return;
      if (index == 0) {
        photoUploadFailed = false; photoUploadNumber = "";
        if (!request->hasParam("number")) { photoUploadFailed = true; return; }
        photoUploadNumber = request->getParam("number")->value();
        Student student;
        if (!findStudentByNumber(photoUploadNumber, student) || !filename.endsWith(".jpg") && !filename.endsWith(".jpeg") && !filename.endsWith(".JPG") && !filename.endsWith(".JPEG")) { photoUploadFailed = true; return; }
        if (!SD.exists("/photos")) SD.mkdir("/photos");
        String path = photoPathFor(photoUploadNumber);
        if (SD.exists(path)) SD.remove(path);
        photoUploadFile = SD.open(path, FILE_WRITE);
        if (!photoUploadFile) photoUploadFailed = true;
      }
      if (!photoUploadFailed && index + len <= MAX_PHOTO_BYTES) photoUploadFile.write(data, len);
      else if (index + len > MAX_PHOTO_BYTES) photoUploadFailed = true;
      if (final && photoUploadFile) photoUploadFile.close();
    });
  server.on("/api/reset-admin/capture", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireWebLogin(request)) return;
    if (adminCardToken.isEmpty() && adminUid.isEmpty()) { request->send(400, "application/json", "{\"error\":\"Önce admin kartı tanımlanmalı.\"}"); return; }
    resetAdminConfirmed = false; resetAdminConfirmedAt = 0; resetAdminScanPending = true; pendingUid = ""; lastUid = "";
    lastMessage = "Sifirlama icin admin kart bekleniyor";
    showLcd("Sifirlama onayi", "Admin kart okut");
    request->send(200, "application/json", "{\"ok\":true}");
  });
  AsyncCallbackJsonWebHandler* factoryReset = new AsyncCallbackJsonWebHandler("/api/reset-all", [](AsyncWebServerRequest* request, JsonVariant& json) {
    if (!requireWebLogin(request)) return;
    JsonObject data = json.as<JsonObject>();
    String confirmation = data["confirmation"] | "";
    String password = data["password"] | "";
    if (confirmation != "SIFIRLA") { request->send(400, "application/json", "{\"error\":\"Onay için SIFIRLA yazılmalı.\"}"); return; }
    if (password != webPassword) { request->send(403, "application/json", "{\"error\":\"Admin paneli parolası hatalı.\"}"); return; }
    if (!resetAdminConfirmed || millis() - resetAdminConfirmedAt > 60000) { resetAdminConfirmed = false; request->send(403, "application/json", "{\"error\":\"Önce fiziksel admin kartı okutulmalı (onay 60 saniye geçerli).\"}"); return; }
    if (!resetAllData()) { request->send(500, "application/json", "{\"error\":\"SD kart sıfırlanamadı.\"}"); return; }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  server.addHandler(factoryReset);
  AsyncCallbackJsonWebHandler* students = new AsyncCallbackJsonWebHandler("/api/students", [](AsyncWebServerRequest* req, JsonVariant& json) { if (requireWebLogin(req)) addStudent(req, json); });
  server.addHandler(students);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_READY, OUTPUT); pinMode(LED_SUCCESS, OUTPUT); pinMode(LED_ERROR, OUTPUT); pinMode(LED_INTERNET, OUTPUT); pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_READY, HIGH); digitalWrite(LED_SUCCESS, LOW); digitalWrite(LED_ERROR, LOW); digitalWrite(LED_INTERNET, LOW);
  signalBoot();
  Wire.begin(LCD_SDA, LCD_SCL); scanI2cDevices(); lcd.init();
  loadTurkishGlyphs();
  lcd.backlight(); showLcd("Pansiyon", "Başlatılıyor...");
  // Ortak SPI hattinda iki cihazin CS pinini ilk anda pasif tut.
  pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  pinMode(RFID_SS, OUTPUT); digitalWrite(RFID_SS, HIGH);
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RC522 tanisi:");
  rfid.PCD_DumpVersionToSerial();
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, sdSpi);
  if (!sdReady) { showLcd("SD kart hatasi", "Baglantiyi kontrol"); }
  else {
    if (!SD.exists(STUDENTS_FILE)) { File f = SD.open(STUDENTS_FILE, FILE_WRITE); f.println("number;name;school;grade;section;room;permitted"); f.close(); }
    if (!SD.exists(CARDS_FILE)) { File f = SD.open(CARDS_FILE, FILE_WRITE); f.println("uid;number"); f.close(); }
    if (!SD.exists(EVENTS_FILE)) { File f = SD.open(EVENTS_FILE, FILE_WRITE); f.println("date;time;uid;student_no;event"); f.close(); }
    if (!SD.exists(LEAVES_FILE)) { File f = SD.open(LEAVES_FILE, FILE_WRITE); f.println("number;start;end"); f.close(); }
    if (!SD.exists(GUESTS_FILE)) { File f = SD.open(GUESTS_FILE, FILE_WRITE); f.println("uid;name;expires;used"); f.close(); }
    if (!SD.exists(CREDENTIALS_FILE)) { File f = SD.open(CREDENTIALS_FILE, FILE_WRITE); f.println("token;number;name;kind"); f.close(); }
    migrateLegacyRecords();
    loadSettings();
    systemUnlocked = !bootLocked;
  }
  // Normal ağa bağlanırken kendi erişim noktası da her zaman açık kalsın.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Pansiyon-Yoklama", AP_PASSWORD);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) delay(250);
  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
  setupWeb();
  showLcd("Kartinizi okutun", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
}

void loop() {
  if (restartScheduled && millis() >= restartAt) ESP.restart();
  updateSystemLed();
  updateWebPanelIndicator();
  updateLcdWelcome();
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    recordCard(); rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  }
}
