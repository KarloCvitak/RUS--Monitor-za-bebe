/**
 * @file main.cpp
 * @author Karlo Cvitak
 * @brief Implementacija sustava za nadzor beba s podrškom za sleep mod i očuvanjem WiFi veze
 * @version 1.5
 * @date 2025-04-14
 * 
 * @details Ovaj sustav omogućuje nadzor temperature, vlažnosti i zvuka djeteta
 * te šalje upozorenja putem Discord webhooks kada se otkriju određeni uvjeti.
 * Koristi DHT22 senzor za precizno mjerenje temperature i vlažnosti.
 * Implementiran light sleep mod za uštedu energije uz očuvanje WiFi veze.
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
 
 // Mrežne postavke
 const char* ssid = "Wokwi-GUEST";             ///< WiFi SSID
 const char* password = "";          ///< WiFi lozinka
 const char* webhookUrl = "https://discord.com/api/webhooks/1360323975539851270/coC7e1d5N0uV-GW0PYnUN4a_5VsZkWdDr_p3vBIvGBatiTPczgJmhus0i9FetvZ7_aJo"; ///< URL za Discord webhook
 
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
 const int CRYING_THRESHOLD = 2048; ///< Prag za detekciju plača
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
 
 // RTC varijable za očuvanje podataka tijekom sleep moda
 RTC_DATA_ATTR int bootCount = 0;
 RTC_DATA_ATTR unsigned long lastStatusReportTime = 0;
 RTC_DATA_ATTR unsigned long runningTime = 0;
 RTC_DATA_ATTR int normalReadingsCount = 0;   ///< Brojač normalnih očitanja - premješteno u RTC memoriju
 unsigned long startTime = 0;
 
 // NTP postavke za vremensku oznaku
 const char* ntpServer = "pool.ntp.org";
 const long gmtOffset_sec = 3600;
 const int daylightOffset_sec = 3600;
 
 // Dodatne RTC varijable za debagiranje
 RTC_DATA_ATTR uint8_t wakeupReason = 0;  ///< Razlog buđenja iz sleep moda
 
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
 
 /**
  * @brief Inicijalizira Wi-Fi vezu
  * @return bool True ako je povezivanje uspjelo
  */
 bool setupWiFi() {
   if (WiFi.status() == WL_CONNECTED) {
     Serial.println("WiFi već povezan");
     return true;
   }
 
   Serial.println("Povezivanje na WiFi...");
   WiFi.begin(ssid, password, 6);
   
   int attempts = 0;
   while (WiFi.status() != WL_CONNECTED && attempts < 40) {
     delay(500);
     Serial.print(".");
     attempts++;
   }
   
   if (WiFi.status() == WL_CONNECTED) {
     Serial.println("");
     Serial.println("WiFi povezan");
     Serial.println("IP adresa: ");
     Serial.println(WiFi.localIP());
     setupTime();
     return true;
   } else {
     Serial.println("");
     Serial.println("WiFi povezivanje neuspješno!");
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
     // Računamo ukupno vrijeme rada
     unsigned long totalRunTime = runningTime + (currentTime - startTime);
    
     // Šalje status izvještaj svakih 30 minuta
     if (totalRunTime - lastStatusReportTime > 1800000) {
         String message = "📊 Redoviti izvještaj\n";
         message += "Temperatura: " + String(currentTemp, 1) + " °C\n";
         message += "Vlažnost: " + String(currentHumidity, 1) + " %\n";
         message += "Zvuk: " + String(soundLevel) + "\n";
         message += "Status: " + String(isCrying ? "Beba plače" : "Tiho") + ", ";
         message += String(tempOutOfRange ? "Temperatura izvan raspona" : "Temperatura u redu") + ", ";
         message += String(humidityOutOfRange ? "Vlažnost izvan raspona" : "Vlažnost u redu") + "\n";
         message += "Vrijeme rada: " + String(totalRunTime / 60000) + " minuta\n";
         message += "Normalna očitanja: " + String(normalReadingsCount) + "/" + String(MAX_NORMAL_READINGS);
        
         // Zelena boja za redoviti izvještaj
         if (sendDiscordAlert(message, 5763719)) {
             lastStatusReportTime = totalRunTime;
             // Ažuriramo RTC varijable prije mogućeg sleep-a
             runningTime = totalRunTime;
         }
     }
 }
 
 /**
  * @brief Vraća string za razlog buđenja
  * @param wakeup_reason ESP32 razlog buđenja iz sleep-a
  * @return String Opis razloga buđenja
  */
 String getWakeupReasonString(esp_sleep_wakeup_cause_t wakeup_reason) {
     switch(wakeup_reason) {
         case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
         case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
         case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
         case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
         case ESP_SLEEP_WAKEUP_ULP: return "ULP";
         case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
         case ESP_SLEEP_WAKEUP_UART: return "UART";
         default: return "NEPOZNATO";
     }
 }
 
 /**
  * @brief Ulazak u light sleep mod koji čuva WiFi vezu
  * @param sleepDuration Trajanje spavanja u mikrosekundama
  */
 void enterLightSleep(uint64_t sleepDuration) {
     Serial.println("Ulazak u light sleep mod uz očuvanje WiFi veze...");
     Serial.println("Broj normalnih očitanja prije sleep-a: " + String(normalReadingsCount));
     Serial.flush();
     
     // Spremamo trenutnu vrijednost vremena rada prije spavanja
     runningTime += (millis() - startTime);
     
     // Postavke za light sleep - očuvanje WiFi veze, ali bez automatskog prekida veze
     WiFi.setSleep(false);  // Postaviti WiFi u aktivan način
     esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
     
     // Isključi vanjsko buđenje ako idemo u normalni sleep
     if (sleepDuration == SLEEP_DURATION_NORMAL) {
         // Za dulji sleep, oslanjamo se samo na timer
         // Ne želimo da zvuk prekida duži sleep
         esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
     } else {
         // Za kraći sleep, dopuštamo buđenje i zbog zvuka
         esp_sleep_enable_ext0_wakeup((gpio_num_t)SOUND_SENSOR_PIN, HIGH);
     }
     
     // Postavite timer za automatsko buđenje nakon zadanog vremena
     esp_sleep_enable_timer_wakeup(sleepDuration);
     
     // Ulazak u light sleep mod
     esp_light_sleep_start();
     
     // Kod se nastavlja odavde nakon buđenja iz light sleep-a
     esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
     wakeupReason = (uint8_t)wakeup_reason;  // Spremi razlog buđenja za debagiranje
     
     Serial.println("Probuđen iz light sleep moda, razlog: " + getWakeupReasonString(wakeup_reason));
     
     // Ako je buđenje zbog zvuka, resetiramo brojač normalnih očitanja
     if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
         Serial.println("Buđenje zbog zvuka, resetiranje brojača normalnih očitanja");
         normalReadingsCount = 0;
     }
     
     // Resetiramo početno vrijeme za ovu sesiju
     startTime = millis();
 }
 
 void setup() {
     Serial.begin(115200);
     delay(1000);  // Kratka pauza za stabilizaciju serijske veze
     
     // Provjera razloga buđenja
     esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
     
     // Povećanje brojača pokretanja (samo ako je stvarno reset, ne za buđenje iz light sleep-a)
     if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER && wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {
         bootCount++;
         normalReadingsCount = 0;  // Resetiraj brojač kod stvarnog boot-a
         Serial.println("Novi boot, broj pokretanja: " + String(bootCount));
     } else {
         Serial.println("Buđenje iz sleep moda, razlog: " + getWakeupReasonString(wakeup_reason));
         Serial.println("Broj pokretanja ostaje: " + String(bootCount));
         Serial.println("Broj normalnih očitanja: " + String(normalReadingsCount));
     }
     
     // Postavljanje početnog vremena za ovu sesiju
     startTime = millis();
     
     // Postavljanje pinova
     pinMode(STATUS_LED_PIN, OUTPUT);
     pinMode(SOUND_SENSOR_PIN, INPUT);
     
     // Inicijalizacija DHT senzora
     dht.begin();
 
     // Inicijalizacija mrežnih usluga
     bool wifiConnected = setupWiFi();
     int wifiAttempts = 0;
     while(!wifiConnected && wifiAttempts < 5){
       wifiConnected = setupWiFi();
       wifiAttempts++;
       delay(1000);
     }
 
     // ⏳ Čekaj dok se ne dobije NTP vrijeme samo pri prvom pokretanju
     if (bootCount == 1) {
       struct tm timeinfo;
       int ntpAttempts = 0;
       while (!getLocalTime(&timeinfo) && ntpAttempts < 10) {
         Serial.println("⏳ Čekam vrijeme s NTP-a...");
         delay(500);
         ntpAttempts++;
       }
     }
   
     Serial.println("Sustav spreman za nadzor!");
     
     // Inicijalno upozorenje samo kod prvog pokretanja ili nakon reset-a
     if (bootCount == 1 || (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER &&
                           wakeup_reason != ESP_SLEEP_WAKEUP_EXT0)) {
         sendDiscordAlert("✅ Monitor za bebe je aktivan i započinje nadzor (temperatura, vlažnost i zvuk)", 5763719);
     }
   
     // Signalizacija da je sustav pokrenut
     for (int i = 0; i < 3; i++) {
         digitalWrite(STATUS_LED_PIN, HIGH);
         delay(100);
         digitalWrite(STATUS_LED_PIN, LOW);
         delay(100);
     }
 }
 
 void loop() {
     // Indikacija da je sustav aktivan
     digitalWrite(STATUS_LED_PIN, HIGH);
     
     // Osiguravamo da je WiFi povezan
     ensureWiFiConnected();
     
     bool anyAlert = false;
     
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
     
     // Isključivanje LED indikatora
     digitalWrite(STATUS_LED_PIN, LOW);
     
     // Debugging na serijsku konzolu
     Serial.println("Status sustava:");
     Serial.println("- Brojač normalnih očitanja: " + String(normalReadingsCount) + "/" + String(MAX_NORMAL_READINGS));
     Serial.println("- Upozorenja: " + String(anyAlert ? "DA" : "NE"));
     Serial.println("- Razlog zadnjeg buđenja: " + getWakeupReasonString((esp_sleep_wakeup_cause_t)wakeupReason));
     
     // Odlučivanje o ulasku u sleep mod
     if (!anyAlert) {
         normalReadingsCount++;
         
         if (normalReadingsCount >= MAX_NORMAL_READINGS) {
             // Dovoljno normalnih očitanja, ulazak u duži sleep
             Serial.println("Dovoljno normalnih očitanja (" + String(normalReadingsCount) + "), ulazak u duži sleep");
             
             // Šalje debug poruku prije ulaska u duži sleep
             String message = "🔍 DEBUG: Ulazak u duži sleep\n";
             message += "Broj normalnih očitanja: " + String(normalReadingsCount) + "\n";
             message += "Sleep trajanje: " + String(SLEEP_DURATION_NORMAL / 1000000) + " sekundi";
             sendDiscordAlert(message, 16753920);  // Narančasta boja za debug
             
             // Resetiraj brojač nakon što smo odlučili ići u duži sleep
             normalReadingsCount = 0;
             
             // Ulazak u duži sleep
             enterLightSleep(SLEEP_DURATION_NORMAL);
         } else {
             // Još uvijek pratimo da vidimo je li stanje stabilno
             Serial.println("Normalno čitanje #" + String(normalReadingsCount) + " od " + String(MAX_NORMAL_READINGS));
             delay(2000);
         }
     } else {
         // Resetiranje brojača normalnih očitanja
         normalReadingsCount = 0;
         Serial.println("Detektirano upozorenje, resetiranje brojača normalnih očitanja");
         
         // Kraći sleep jer imamo upozorenja
         Serial.println("Detektirano upozorenje, ostajemo u aktivnom načinu rada");
         
         // Kraća pauza između provjera u slučaju upozorenja
         delay(2000);
     }
 }
 