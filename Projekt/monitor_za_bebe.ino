/**
 * @file main.cpp
 * @author Karlo Cvitak
 * @brief Implementacija sustava za nadzor beba s podrškom za sleep mod
 * @version 1.2
 * @date 2025-04-12
 * 
 * @details Ovaj sustav omogućuje nadzor temperature, vlažnosti i zvuka djeteta
 * te šalje upozorenja putem Discord webhooks kada se otkriju određeni uvjeti.
 * Koristi DHT22 senzor za precizno mjerenje temperature i vlažnosti.
 * Implementiran sleep mod za uštedu energije kada su parametri u normalnom rasponu.
 */

#include <Arduino.h>
#include <DHT.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/rtc_io.h"
#include <WiFiClientSecure.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// Mrežne postavke
const char* ssid = "Wokwi-GUEST";             ///< WiFi SSID
const char* password = "";          ///< WiFi lozinka
const char* webhookUrl = "https://discord.com/api/webhooks/1360323975539851270/coC7e1d5N0uV-GW0PYnUN4a_5VsZkWdDr_p3vBIvGBatiTPczgJmhus0i9FetvZ7_aJo"; ///< URL za Discord webhook

#define SCREEN_WIDTH 128 // OLED display sirina, iu pikselima
#define SCREEN_HEIGHT 64 // OLED display visina, u pikselima

// Definicije pinova
#define DHT_PIN 4          ///< Pin za DHT22 senzor
#define SOUND_SENSOR_PIN 34 ///< Pin za zvučni senzor
#define STATUS_LED_PIN 2    ///< Pin za LED indikator statusa

// Definiranje tipa DHT senzora
#define DHTTYPE DHT22     ///< DHT22 (AM2302) senzor

// Konstante za nadzor
const float MIN_TEMP = 18.0;    ///< Minimalna sigurna temperatura u °C
const float MAX_TEMP = 26.0;    ///< Maksimalna sigurna temperatura u °C
const float MIN_HUMIDITY = 40.0; ///< Minimalna sigurna vlažnost u %
const float MAX_HUMIDITY = 60.0; ///< Maksimalna sigurna vlažnost u %
const int CRYING_THRESHOLD = 2200; ///< Prag za detekciju plača
const unsigned long ALERT_COOLDOWN = 60000; ///< Vremenski razmak između upozorenja (1 minuta)

// Sleep mod konstante
const uint64_t SLEEP_DURATION_NORMAL = 300000000; // 5 minuta u mikrosekundama
const uint64_t SLEEP_DURATION_ALERT = 10000000;   // 10 sekundi u mikrosekundama
const int MAX_NORMAL_READINGS = 3;                // Broj normalnih očitanja prije ulaska u duži sleep

// Globalne varijable
DHT dht(DHT_PIN, DHTTYPE);    ///< DHT objekt za mjerenje temperature i vlažnosti
unsigned long lastCryingAlert = 0;  ///< Vrijeme zadnjeg upozorenja za plač
unsigned long lastTempAlert = 0;    ///< Vrijeme zadnjeg upozorenja za temperaturu
unsigned long lastHumidityAlert = 0; ///< Vrijeme zadnjeg upozorenja za vlažnost
float currentTemp = 0.0;       ///< Trenutna temperatura
float currentHumidity = 0.0;   ///< Trenutna vlažnost
int soundLevel = 0;            ///< Trenutna razina zvuka
bool isCrying = false;         ///< Status plača
bool tempOutOfRange = false;   ///< Status temperature van raspona
bool humidityOutOfRange = false; ///< Status vlažnosti van raspona
int normalReadingsCount = 0;   ///< Brojač normalnih očitanja

// RTC varijable za očuvanje podataka tijekom sleep moda
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long lastStatusReport = 0;

// NTP postavke za vremensku oznaku
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Dodatne RTC varijable za debagiranje
RTC_DATA_ATTR uint8_t wakeupReason = 0;  ///< Razlog buđenja iz sleep moda

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Globalna varijabla za praćenje statusa OLED-a
bool oledInitialized = false;

/**
 * @brief Inicijalizira OLED zaslon
 * @return bool Status inicijalizacije
 */
bool initOLED() {
  Serial.println("Inicijalizacija OLED zaslona...");
  
  // Provjera je li OLED već inicijaliziran
  if (oledInitialized) {
    Serial.println("OLED već inicijaliziran");
    return true;
  }
  
  // Zaštićeni pokušaj inicijalizacije
  bool success = false;
  try {
    // Inicijalizacija OLED-a
    success = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (success) {
      oledInitialized = true;
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Inicijalizacija...");
      display.display();
      Serial.println("OLED inicijaliziran uspješno");
    } else {
      Serial.println("Greška pri inicijalizaciji OLED-a");
    }
  } catch (...) {
    Serial.println("Iznimka tijekom inicijalizacije OLED-a");
    success = false;
  }
  
  return success;
}


/**
 * @brief Sigurna funkcija za prikaz teksta na OLED zaslonu
 * @param text Tekst za prikaz
 */
void safeDisplayText(String text) {
  if (!oledInitialized) {
    if (!initOLED()) {
      Serial.println("Ne mogu prikazati tekst - OLED nije inicijaliziran");
      return;
    }
  }
  
  try {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(text);
    display.display();
  } catch (...) {
    Serial.println("Greška pri prikazu teksta na OLED-u");
  }
}

/**
 * @brief Prikazuje poruku o povezivanju na WiFi na OLED zaslonu
 */
void displayWiFiConnecting() {
  if (!oledInitialized) {
    if (!initOLED()) {
      Serial.println("Ne mogu prikazati WiFi status - OLED nije inicijaliziran");
      return;
    }
  }
  
  try {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Povezivanje na WiFi...");
    display.setCursor(0, 16);
    display.println(ssid);
    display.drawRect(0, 30, 128, 10, WHITE);
    display.display();
  } catch (...) {
    Serial.println("Greška pri prikazu WiFi statusa na OLED-u");
  }
}

/**
 * @brief Ažurira prikaz napretka spajanja na WiFi
 * @param progress Vrijednost napretka (0-100)
 */
void updateWiFiProgress(int progress) {
  if (!oledInitialized) return;
  
  try {
    // Osiguravamo da je progress unutar 0-100
    progress = constrain(progress, 0, 100);
    display.fillRect(2, 32, progress * 124 / 100, 6, WHITE);
    display.display();
  } catch (...) {
    Serial.println("Greška pri ažuriranju WiFi napretka na OLED-u");
  }
}

/**
 * @brief Prikazuje informacije o uspješnom povezivanju na WiFi
 * @param ip IP adresa
 */
void displayWiFiConnected(String ip) {
  if (!oledInitialized) {
    if (!initOLED()) return;
  }
  
  try {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("WiFi povezan!");
    display.setCursor(0, 16);
    display.println("SSID: " + String(ssid));
    display.setCursor(0, 32);
    display.println("IP: " + ip);
    display.display();
    delay(2000);
  } catch (...) {
    Serial.println("Greška pri prikazu WiFi povezan na OLED-u");
  }
}

/**
 * @brief Prikazuje trenutnu temperaturu i vlažnost na OLED zaslonu
 * @param temp Temperatura u °C
 * @param humidity Vlažnost u %
 * @param soundLevel Razina zvuka
 */
void displaySensorData(float temp, float humidity, int soundLevel) {
  if (!oledInitialized) {
    if (!initOLED()) return;
  }
  
  // Provjera ispravnosti podataka
  if (isnan(temp) || isnan(humidity)) {
    safeDisplayText("Greška u očitanju senzora!");
    return;
  }
  
  try {
    display.clearDisplay();
    
    // Naslov
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Monitor za bebe");
    display.drawLine(0, 10, 128, 10, WHITE);
    
    // Temperatura
    display.setTextSize(2);
    display.setCursor(0, 14);
    display.print(String(temp, 1));
    display.setTextSize(1);
    display.print(" C");
    
    // Simbol temperature
    display.drawCircle(62, 18, 2, WHITE);
    
    // Vlažnost
    display.setTextSize(2);
    display.setCursor(72, 14);
    display.print(String(humidity, 0));
    display.setTextSize(1);
    display.print(" %");
    
    // Status temperature
    display.setTextSize(1);
    display.setCursor(0, 35);
    if (temp < MIN_TEMP) {
      display.println("Temp: PRENISKA!");
    } else if (temp > MAX_TEMP) {
      display.println("Temp: PREVISOKA!");
    } else {
      display.println("Temp: U REDU");
    }
    
    // Status vlažnosti
    display.setCursor(0, 45);
    if (humidity < MIN_HUMIDITY) {
      display.println("Vlaga: PRENISKA!");
    } else if (humidity > MAX_HUMIDITY) {
      display.println("Vlaga: PREVISOKA!");
    } else {
      display.println("Vlaga: U REDU");
    }
    
    // Status zvuka
    display.setCursor(0, 55);
    if (soundLevel > CRYING_THRESHOLD) {
      display.println("Zvuk: PLAČ!");
    } else {
      display.println("Zvuk: Tiho");
    }
    
    display.display();
  } catch (...) {
    Serial.println("Greška pri prikazu podataka senzora na OLED-u");
  }
}

/**
 * @brief Prikazuje informaciju o spavanju na OLED zaslonu
 * @param sleepDuration Trajanje spavanja u sekundama
 */
void displaySleepInfo(uint64_t sleepDuration) {
  if (!oledInitialized) {
    if (!initOLED()) return;
  }
  
  try {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Ulazak u sleep mod");
    display.setCursor(0, 16);
    display.println("Trajanje: " + String(sleepDuration / 1000000) + " s");
    display.setCursor(0, 32);
    display.println("Normalna očitanja:");
    display.setCursor(0, 42);
    display.println(String(normalReadingsCount) + "/" + String(MAX_NORMAL_READINGS));
    display.display();
    delay(1000);
  } catch (...) {
    Serial.println("Greška pri prikazu sleep info na OLED-u");
  }
}


/**
 * @brief Postavlja vrijeme s NTP poslužitelja
 */
void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

/**
 * @brief Dobiva trenutno vrijeme kao string
 * @return String formatiranog vremena i datuma
 */
String getTimeStamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    return "Vrijeme nedostupno";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d-%m-%Y %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

bool setupWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi već povezan");
    safeDisplayText("WiFi već povezan\nIP: " + WiFi.localIP().toString());
    delay(1000);
    return true;
  }

  Serial.println("Povezivanje na WiFi...");
  displayWiFiConnecting();
  WiFi.begin(ssid, password, 6);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    updateWiFiProgress(attempts * 2.5); // 40 pokušaja = 100%
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi povezan");
    Serial.println("IP adresa: ");
    Serial.println(WiFi.localIP());
    displayWiFiConnected(WiFi.localIP().toString());
    setupTime();
    return true;
  } else {
    Serial.println("");
    Serial.println("WiFi povezivanje neuspješno!");
    safeDisplayText("WiFi povezivanje\nNEUSPJEŠNO!");
    delay(2000);
    return false;
  }
}

/**
 * @brief Provjerava Wi-Fi vezu i ponovno se povezuje ako je potrebno
 * @return bool True ako je WiFi povezan
 */
bool ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    return setupWiFi();
  }
  return true;
}

/**
 * @brief Šalje upozorenje na Discord
 * @param message Poruka koja će biti poslana
 * @param color Boja embeda (Discord format)
 * @return bool Status uspjeha slanja
 */
bool sendDiscordAlert(String message, int color) {
  if (!ensureWiFiConnected()) {
    Serial.println("Nemoguće poslati upozorenje zbog nedostatka WiFi veze");
    return false;
  }

  int retryCount = 0;
  const int maxRetries = 3;
  
  while (retryCount < maxRetries) {
    WiFiClientSecure client;
    client.setInsecure();  // ⚠️ Skip certificate validation - insecure but works for testing
    client.setTimeout(10); // Povećaj timeout na 10 sekundi

    HTTPClient http;
    http.begin(client, webhookUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000); // Postavi timeout na 10 sekundi
    
    // Kreiranje JSON poruke za Discord webhook
    DynamicJsonDocument doc(1024);
    doc["content"] = ""; // Opcijska glavna poruka
    
    JsonArray embeds = doc.createNestedArray("embeds");
    JsonObject embed = embeds.createNestedObject();
    embed["title"] = "Upozorenje monitora za bebe";
    embed["description"] = message;
    embed["color"] = color;
    
    JsonObject footer = embed.createNestedObject("footer");
    footer["text"] = "Vrijeme: " + getTimeStamp() + " | Pokretanja: " + String(bootCount);
    
    String jsonString;
    serializeJson(doc, jsonString);
    Serial.println("Slanje Discord poruke: " + jsonString);

    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP odgovor: ");
      Serial.println(httpResponseCode);
      http.end();
      delay(2000); // Važno: dodaj pauzu između Discord zahtjeva
      return true;
    } else {
      Serial.print("Greška kod HTTP zahtjeva: ");
      Serial.println(httpResponseCode);
      http.end();
      retryCount++;
      delay(3000 * retryCount); // Povećaj delay s svakim pokušajem
    }
  }
  
  return false;
}

/**
 * @brief Čita temperaturu i vlažnost s DHT22 senzora
 * @return bool Uspješnost čitanja podataka
 */
bool readDHTSensor() {
    // Čitanje podataka s DHT22 senzora
    currentHumidity = dht.readHumidity();
    currentTemp = dht.readTemperature();
   
    // Provjera je li čitanje uspješno
    if (isnan(currentHumidity) || isnan(currentTemp)) {
        Serial.println("Greška pri čitanju s DHT senzora!");
        return false;
    }
   
    Serial.print("Temperatura: ");
    Serial.print(currentTemp);
    Serial.print(" °C, Vlažnost: ");
    Serial.print(currentHumidity);
    Serial.println(" %");
   
    return true;
}

/**
 * @brief Čita razinu zvuka sa zvučnog senzora
 * @return int Razina zvuka
 */
int readSoundLevel() {
    int value = analogRead(SOUND_SENSOR_PIN);
    Serial.print("Razina zvuka: ");
    Serial.println(value);
    return value;
}

/**
 * @brief Provjerava temperaturu i šalje upozorenje ako je izvan raspona
 * @return bool True ako je temperatura izvan raspona
 */
bool checkTemperature() {
    bool isCurrentlyOutOfRange = (currentTemp < MIN_TEMP || currentTemp > MAX_TEMP);
    unsigned long currentTime = millis();

    if (isCurrentlyOutOfRange &&
        (tempOutOfRange != isCurrentlyOutOfRange || currentTime - lastTempAlert > ALERT_COOLDOWN)) {

        String message;
        if (currentTemp < MIN_TEMP) {
            message = "⚠️ Temperatura je preniska: " + String(currentTemp, 1) + " °C (sigurno: " + String(MIN_TEMP, 1) + " - " + String(MAX_TEMP, 1) + " °C)";
        } else {
            message = "⚠️ Temperatura je previsoka: " + String(currentTemp, 1) + " °C (sigurno: " + String(MIN_TEMP, 1) + " - " + String(MAX_TEMP, 1) + " °C)";
        }

        if (sendDiscordAlert(message, 15158332)) {
            lastTempAlert = currentTime;
            delay(1500); // Dodana pauza između poruka
        }
    }

    tempOutOfRange = isCurrentlyOutOfRange;
    return isCurrentlyOutOfRange;
}

/**
 * @brief Provjerava vlažnost i šalje upozorenje ako je izvan raspona
 * @return bool True ako je vlažnost izvan raspona
 */
bool checkHumidity() {
    bool isCurrentlyOutOfRange = (currentHumidity < MIN_HUMIDITY || currentHumidity > MAX_HUMIDITY);
   
    // Provjera je li vrijeme za slanje upozorenja
    unsigned long currentTime = millis();
    if (isCurrentlyOutOfRange && 
        (humidityOutOfRange != isCurrentlyOutOfRange || 
        currentTime - lastHumidityAlert > ALERT_COOLDOWN)) {
       
        String message;
        if (currentHumidity < MIN_HUMIDITY) {
            message = "💧 Vlažnost je preniska: " + String(currentHumidity, 1) + " % (sigurno: " + String(MIN_HUMIDITY, 1) + " - " + String(MAX_HUMIDITY, 1) + " %)";
        } else {
            message = "💧 Vlažnost je previsoka: " + String(currentHumidity, 1) + " % (sigurno: " + String(MIN_HUMIDITY, 1) + " - " + String(MAX_HUMIDITY, 1) + " %)";
        }
       
        // Plava boja za upozorenje o vlažnosti
        if (sendDiscordAlert(message, 3066993)) {
            lastHumidityAlert = currentTime;
        }
    }
   
    humidityOutOfRange = isCurrentlyOutOfRange;
    return isCurrentlyOutOfRange;
}

/**
 * @brief Provjerava zvuk i šalje upozorenje ako beba plače
 * @return bool True ako beba plače
 */
bool checkSound() {
    soundLevel = readSoundLevel();
    bool isCurrentlyCrying = soundLevel > CRYING_THRESHOLD;
   
    // Provjera je li vrijeme za slanje upozorenja
    unsigned long currentTime = millis();
    if (isCurrentlyCrying && 
        (isCrying != isCurrentlyCrying || 
        currentTime - lastCryingAlert > ALERT_COOLDOWN)) {
       
        String message = "🔊 Beba plače! Razina zvuka: " + String(soundLevel);
       
        // Ljubičasta boja za upozorenje o plaču
        if (sendDiscordAlert(message, 10181046)) {
            lastCryingAlert = currentTime;
        }
    }
   
    isCrying = isCurrentlyCrying;
    return isCurrentlyCrying;
}

/**
 * @brief Periodički izvještaj o stanju
 */
void sendStatusReport() {
    unsigned long currentTime = millis();
    unsigned long totalTime = bootCount * (ESP.getMaxAllocHeap() / 1000) + currentTime; // Aproksimacija ukupnog vremena
   
    // Šalje status izvještaj svakih 30 minuta
    if (totalTime - lastStatusReport > 1800000) {
        String message = "📊 Redoviti izvještaj\n";
        message += "Temperatura: " + String(currentTemp, 1) + " °C\n";
        message += "Vlažnost: " + String(currentHumidity, 1) + " %\n";
        message += "Zvuk: " + String(soundLevel) + "\n";
        message += "Status: " + String(isCrying ? "Beba plače" : "Tiho") + ", ";
        message += String(tempOutOfRange ? "Temperatura izvan raspona" : "Temperatura u redu") + ", ";
        message += String(humidityOutOfRange ? "Vlažnost izvan raspona" : "Vlažnost u redu");
       
        // Zelena boja za redoviti izvještaj
        if (sendDiscordAlert(message, 5763719)) {
            lastStatusReport = totalTime;
        }
    }
}

/**
 * @brief Ulazak u deep sleep mod
 * @param sleepDuration Trajanje spavanja u mikrosekundama
 */
void enterDeepSleep(uint64_t sleepDuration) {
    Serial.println("Ulazak u deep sleep mod...");
    Serial.flush();

    // Ako NE želiš da budi na šum:
    // esp_sleep_enable_ext0_wakeup((gpio_num_t)SOUND_SENSOR_PIN, HIGH);   // UKLONI OVO!

    esp_sleep_enable_timer_wakeup(sleepDuration);

    // Sleep!
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);

    
    // Postavljanje pinova
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(SOUND_SENSOR_PIN, INPUT);
    
    // Inicijalizacija DHT senzora
    dht.begin();

      // Inicijalizacija mrežnih usluga
    displayWiFiConnecting();
    bool wifiConnected = setupWiFi();
    int wifiAttempts = 0;
    while(!wifiConnected && wifiAttempts < 5){
      wifiConnected = setupWiFi();
      wifiAttempts++;
      delay(1000);
    
    Serial.println("Sustav spreman za nadzor!");
    
    // Signalizacija da je sustav pokrenut
    for (int i = 0; i < 3; i++) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(100);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(100);
    }
}
}

void loop() {
    // Indikacija da je sustav aktivan
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    bool anyAlert = false;

       // Osiguravamo da je WiFi povezan
    ensureWiFiConnected();
    
    // Provjera senzora
    if (readDHTSensor()) {
        bool tempAlert = checkTemperature();
        bool humAlert = checkHumidity();
        anyAlert = tempAlert || humAlert;
    }
    
    bool cryingAlert = checkSound();
    anyAlert = anyAlert || cryingAlert;
    
    // Slanje periodičkog izvještaja
    sendStatusReport();

    displaySensorData(currentTemp, currentHumidity, soundLevel);
    
    // Isključivanje LED indikatora
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // Odlučivanje o ulasku u sleep mod
    if (!anyAlert) {
        normalReadingsCount++;
        
        if (normalReadingsCount >= MAX_NORMAL_READINGS) {
            // Dovoljno normalnih očitanja, ulazak u duži sleep
            normalReadingsCount = 0;
            WiFi.disconnect(true); 
            enterDeepSleep(SLEEP_DURATION_NORMAL);
        } else {
            // Još uvijek pratimo da vidimo je li stanje stabilno
            Serial.println("Normalno čitanje #" + String(normalReadingsCount) + " od " + String(MAX_NORMAL_READINGS));
            delay(2000);
        }
    } else {
        // Resetiranje brojača normalnih očitanja
        normalReadingsCount = 0;
        
        // Kraći sleep jer imamo upozorenja
        Serial.println("Detektirano upozorenje, ostajemo u aktivnom načinu rada");
        
        // Kraća pauza između provjera u slučaju upozorenja
        delay(2000);
        
        // Ako želimo kraći sleep umjesto delay, možemo koristiti:
        // enterDeepSleep(SLEEP_DURATION_ALERT);
    }
}
