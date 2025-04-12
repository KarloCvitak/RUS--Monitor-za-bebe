/**
 * @file monitor_za_bebe.ino
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
#include "driver/rtc_io.h"

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
int normalReadingsCount = 0;   ///< Brojač normalnih očitanja

// RTC varijable za očuvanje podataka tijekom sleep moda
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR unsigned long lastStatusReport = 0;

/**
 * @brief Šalje upozorenje na Discord
 * @param message Poruka koja će biti poslana
 * @param color Boja embeda (Discord format)
 * @return bool Status uspjeha slanja
 */
bool sendDiscordAlert(String message, int color) {
    // Implementacija slanja na Discord
    // Trenutno samo simulacija
    Serial.println("Discord poruka: " + message);
    return true;
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
    
    // Postavite pin za buđenje iz sna (može se koristiti senzor zvuka)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SOUND_SENSOR_PIN, HIGH); // Buđenje kada se detektira zvuk
    
    // Postavite timer za automatsko buđenje nakon zadanog vremena
    esp_sleep_enable_timer_wakeup(sleepDuration);
    
    // Ulazak u deep sleep mod
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    
    // Povećanje brojača pokretanja
    ++bootCount;
    Serial.println("Broj pokretanja: " + String(bootCount));
    
    // Postavljanje pinova
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(SOUND_SENSOR_PIN, INPUT);
    
    // Inicijalizacija DHT senzora
    dht.begin();
    
    Serial.println("Sustav spreman za nadzor!");
    
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
    
    // Odlučivanje o ulasku u sleep mod
    if (!anyAlert) {
        normalReadingsCount++;
        
        if (normalReadingsCount >= MAX_NORMAL_READINGS) {
            // Dovoljno normalnih očitanja, ulazak u duži sleep
            normalReadingsCount = 0;
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
