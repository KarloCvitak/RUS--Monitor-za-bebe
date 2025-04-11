/**
 * @file main.cpp
 * @author Karlo Cvitak
 * @brief Implementacija sustava za nadzor beba
 * @version 1.1
 * @date 2025-04-11
 * 
 * @details Ovaj sustav omogućuje nadzor temperature, vlažnosti i zvuka djeteta
 * te šalje upozorenja putem Discord webhooks kada se otkriju određeni uvjeti.
 * Koristi DHT22 senzor za precizno mjerenje temperature i vlažnosti.
 */
#include <Arduino.h>
#include <DHT.h>

 
 // Definicije pinova
 #define DHT_PIN 4          ///< Pin za DHT22 senzor
 #define SOUND_SENSOR_PIN 35 ///< Pin za zvučni senzor
 #define STATUS_LED_PIN 2    ///< Pin za LED indikator statusa
 
 // Definiranje tipa DHT senzora
 #define DHTTYPE DHT22     ///< DHT22 (AM2302) senzor
 
 // Konstante za nadzor
 const float MIN_TEMP = 18.0;    ///< Minimalna sigurna temperatura u °C
 const float MAX_TEMP = 26.0;    ///< Maksimalna sigurna temperatura u °C
 const float MIN_HUMIDITY = 40.0; ///< Minimalna sigurna vlažnost u %
 const float MAX_HUMIDITY = 60.0; ///< Maksimalna sigurna vlažnost u %
 const int CRYING_THRESHOLD = 2000; ///< Prag za detekciju plača
 const unsigned long ALERT_COOLDOWN = 60000; ///< Vremenski razmak između upozorenja (1 minuta)
 
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
 
 /**
  * @brief Šalje upozorenje na Discord
  * @param message Poruka koja će biti poslana
  * @param color Boja embeda (Discord format)
  * @return bool Status uspjeha slanja
  */
 bool sendDiscordAlert(String message, int color) {

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
  */
 void checkTemperature() {
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
       delay(1500); // ⚠️ dodan delay između poruka
     }
   }
 
   tempOutOfRange = isCurrentlyOutOfRange;
 }
 
 
 /**
  * @brief Provjerava vlažnost i šalje upozorenje ako je izvan raspona
  */
 void checkHumidity() {
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
 }
 
 /**
  * @brief Provjerava zvuk i šalje upozorenje ako beba plače
  */
 void checkSound() {
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
 }
 
 /**
  * @brief Periodički izvještaj o stanju
  */
 void sendStatusReport() {
   static unsigned long lastStatusReport = 0;
   unsigned long currentTime = millis();
   
   // Šalje status izvještaj svakih 30 minuta
   if (currentTime - lastStatusReport > 1800000) {
     String message = "📊 Redoviti izvještaj\n";
     message += "Temperatura: " + String(currentTemp, 1) + " °C\n";
     message += "Vlažnost: " + String(currentHumidity, 1) + " %\n";
     message += "Zvuk: " + String(soundLevel) + "\n";
     message += "Status: " + String(isCrying ? "Beba plače" : "Tiho") + ", ";
     message += String(tempOutOfRange ? "Temperatura izvan raspona" : "Temperatura u redu") + ", ";
     message += String(humidityOutOfRange ? "Vlažnost izvan raspona" : "Vlažnost u redu");
     
     // Zelena boja za redoviti izvještaj
     if (sendDiscordAlert(message, 5763719)) {
       lastStatusReport = currentTime;
     }
   }
 }
 
 void setup() {
   Serial.begin(115200);
   Serial.println("Inicijalizacija sustava za nadzor beba s DHT22 senzorom...");
   
   // Postavljanje pinova
   pinMode(STATUS_LED_PIN, OUTPUT);
   pinMode(SOUND_SENSOR_PIN, INPUT);
   
   // Inicijalizacija DHT senzora
   dht.begin();

   
   Serial.println("Sustav spreman za nadzor!");
 }
 
 
 void loop() {
   // Treperenje LED indikatora kao znak rada
   digitalWrite(STATUS_LED_PIN, HIGH);
   delay(100);
   digitalWrite(STATUS_LED_PIN, LOW);
   
   // Provjera senzora
   if (readDHTSensor()) {
     checkTemperature();
     checkHumidity();
   }
   checkSound();
   
   // Slanje periodičkog izvještaja
   sendStatusReport();
   
   // Pauza između očitanja
   delay(2000);
 }
 
 