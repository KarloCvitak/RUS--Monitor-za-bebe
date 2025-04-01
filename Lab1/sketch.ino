// Arduino Interrupt Management System
// Program demonstrira upravljanje višestrukim prekidima s različitim prioritetima

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Definiranje pinova
#define BUTTON1_PIN 2      // Vanjski prekid INT0
#define BUTTON2_PIN 3      // Vanjski prekid INT1
#define LED1_PIN 8         // LED za button1
#define LED2_PIN 9         // LED za button2
#define LED3_PIN 10        // LED za timer
#define LED4_PIN 11        // LED za ADC
#define TEMP_SENSOR_PIN A0 // Analogni pin za NTC termistor

// Konstante za NTC termistor
#define THERMISTOR_NOMINAL 10000   // Otpor na 25°C
#define TEMPERATURE_NOMINAL 25     // Temperatura za nominalni otpor (25°C)
#define B_COEFFICIENT 3950         // Beta koeficijent termistora
#define SERIES_RESISTOR 10000      // Vrijednost serijskog otpornika (10kΩ)

// Inicijalizacija LCD-a (I2C adresa: 0x27, 16 znakova, 2 reda)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Varijable za upravljanje stanjem
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile bool timerFired = false;
volatile bool adcReady = false;

// Zastavice za detekciju konflikta resursa
volatile bool processingButton1 = false;
volatile bool processingButton2 = false;
volatile bool processingTimer = false;
volatile bool processingADC = false;

// Brojači za praćenje koliko puta je svaki prekid aktiviran
volatile unsigned int button1Count = 0;
volatile unsigned int button2Count = 0;
volatile unsigned int timerCount = 0;
volatile unsigned int adcCount = 0;

// Varijable za demonstraciju konflikta
volatile unsigned int conflictCount = 0;

// Vrijeme zadnjeg prekida (za debouncing tipkala)
volatile unsigned long lastButton1Time = 0;
volatile unsigned long lastButton2Time = 0;
const unsigned long debounceTime = 200; // 200 ms za debouncing

// Trenutno izmjerena vrijednost temperature
volatile int currentADCValue = 0;
volatile float currentTemp = 0.0;

// Prekidna rutina za tipkalo 1 (najviši prioritet - INT0)
void ISR_Button1() {
  unsigned long currentTime = millis();
  
  // Debouncing - ignoriranje više pritisaka u kratkom vremenu
  if (currentTime - lastButton1Time < debounceTime) {
    return;
  }
  
  lastButton1Time = currentTime;
  button1Pressed = true;
  button1Count++;
  
  // Provjera konflikta resursa
  if (processingButton2 || processingTimer || processingADC) {
    conflictCount++;
  }
  
  // Brza akcija unutar ISR - paljenje/gašenje LED-a
  digitalWrite(LED1_PIN, !digitalRead(LED1_PIN));
}

// Prekidna rutina za tipkalo 2 (srednji prioritet - INT1)
void ISR_Button2() {
  unsigned long currentTime = millis();
  
  // Debouncing - ignoriranje više pritisaka u kratkom vremenu
  if (currentTime - lastButton2Time < debounceTime) {
    return;
  }
  
  lastButton2Time = currentTime;
  button2Pressed = true;
  button2Count++;
  
  // Provjera konflikta resursa
  if (processingButton1 || processingTimer || processingADC) {
    conflictCount++;
  }
  
  // Brza akcija unutar ISR - paljenje/gašenje LED-a
  digitalWrite(LED2_PIN, !digitalRead(LED2_PIN));
}

// Prekidna rutina za Timer1 (niži prioritet)
ISR(TIMER1_COMPA_vect) {
  timerFired = true;
  timerCount++;
  
  // Provjera konflikta resursa
  if (processingButton1 || processingButton2 || processingADC) {
    conflictCount++;
  }
  
  // Brza akcija unutar ISR - paljenje/gašenje LED-a
  digitalWrite(LED3_PIN, !digitalRead(LED3_PIN));
  
  // Omogućavanje ugniježđenih prekida (dopušta da prekidi višeg prioriteta prekinu ovaj prekid)
  sei();
  
  // Pokretanje ADC konverzije
  ADCSRA |= (1 << ADSC);
}

// Prekidna rutina za ADC (najniži prioritet)
ISR(ADC_vect) {
  adcReady = true;
  adcCount++;
  
  // Očitavanje rezultata ADC konverzije
  currentADCValue = ADC;
  
  // Provjera konflikta resursa
  if (processingButton1 || processingButton2 || processingTimer) {
    conflictCount++;
  }
  
  // Brza akcija unutar ISR - paljenje/gašenje LED-a
  digitalWrite(LED4_PIN, !digitalRead(LED4_PIN));
}

// Funkcija za izračun temperature iz NTC termistora
float calculateTemperature(int adcValue) {
  // Konverzija ADC vrijednosti u otpor
  float resistance = SERIES_RESISTOR / ((1023.0 / adcValue) - 1.0);
  
  // Steinhart-Hart jednadžba za izračun temperature
  float steinhart;
  steinhart = resistance / THERMISTOR_NOMINAL;          // (R/Ro)
  steinhart = log(steinhart);                           // ln(R/Ro)
  steinhart /= B_COEFFICIENT;                           // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);    // + (1/To)
  steinhart = 1.0 / steinhart;                          // Invertiranje
  steinhart -= 273.15;                                  // Konverzija iz Kelvina u Celzije
  
  return steinhart;
}

// Funkcija za ažuriranje LCD-a
void updateLCD() {
  lcd.clear();
  
  // Prvi red: brojači prekida
  lcd.setCursor(0, 0);
  lcd.print("B1:");
  lcd.print(button1Count);
  lcd.print(" B2:");
  lcd.print(button2Count);
  lcd.print(" T:");
  lcd.print(timerCount);
  
  // Drugi red: temperatura i konflikti
  lcd.setCursor(0, 1);
  lcd.print("Tmp:");
  lcd.print(currentTemp, 1);
  lcd.print("C C:");
  lcd.print(conflictCount);
}

void setup() {
  // Inicijalizacija LCD-a
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Arduino Interrupt");
  lcd.setCursor(0, 1);
  lcd.print("System Starting");

  // Inicijalizacija serijske komunikacije
  Serial.begin(9600);
  Serial.println(F("Arduino Interrupt Management System"));
  Serial.println(F("---------------------------------------"));
  
  // Konfiguracija pinova
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(LED4_PIN, OUTPUT);
  
  // Inicijalno stanje LED-ova
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
  digitalWrite(LED4_PIN, LOW);
  
  // Konfiguracija vanjskih prekida
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), ISR_Button1, FALLING); // INT0 - najviši prioritet
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), ISR_Button2, FALLING); // INT1 - srednji prioritet
  
  // Konfiguracija Timer1 prekida
  noInterrupts(); // Privremeno onemogućavanje prekida tijekom konfiguracije
  
  // Postavljanje Timer1 za prekid svake sekunde
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;                // Postavljanje brojača na 0
  OCR1A = 15624;            // Postavljanje usporedne vrijednosti (16MHz/1024/1Hz - 1)
  TCCR1B |= (1 << WGM12);   // CTC način rada
  TCCR1B |= (1 << CS12) | (1 << CS10); // 1024 prescaler
  TIMSK1 |= (1 << OCIE1A);  // Omogućavanje prekida usporednog podudaranja
  
  // Konfiguracija ADC prekida
  ADMUX = (1 << REFS0); // Referentni napon = AVCC
  ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Omogućavanje ADC, ADC prekida i postavljanje prescalera na 128
  
  interrupts(); // Ponovno omogućavanje prekida
  
  Serial.println(F("Sustav inicijaliziran. Prekidi konfigurirani s prioritetima:"));
  Serial.println(F("1. Button1 (INT0) - najviši prioritet"));
  Serial.println(F("2. Button2 (INT1)"));
  Serial.println(F("3. Timer1"));
  Serial.println(F("4. ADC - najniži prioritet"));
  Serial.println(F("---------------------------------------"));
  
  delay(2000); // Daj vrijeme korisniku da pročita inicijalni LCD prikaz
}

void loop() {
  // Obrada prekida tipkala 1
  if (button1Pressed) {
    processingButton1 = true;
    
    Serial.print(F("Prekid Button1: "));
    Serial.print(button1Count);
    Serial.println(F(" puta"));
    
    // Simulacija neke obrade koja traje
    delay(50);
    
    button1Pressed = false;
    processingButton1 = false;
  }
  
  // Obrada prekida tipkala 2
  if (button2Pressed) {
    processingButton2 = true;
    
    Serial.print(F("Prekid Button2: "));
    Serial.print(button2Count);
    Serial.println(F(" puta"));
    
    // Simulacija neke obrade koja traje
    delay(50);
    
    button2Pressed = false;
    processingButton2 = false;
  }
  
  // Obrada timer prekida
  if (timerFired) {
    processingTimer = true;
    
    Serial.print(F("Timer prekid: "));
    Serial.print(timerCount);
    Serial.println(F(" puta"));
    
    // Simulacija neke obrade koja traje
    delay(50);
    
    timerFired = false;
    processingTimer = false;
  }
  
  // Obrada ADC prekida
  if (adcReady) {
    processingADC = true;
    
    // Pretvaranje RAW ADC vrijednosti u temperaturu koristeći NTC termistor
    currentTemp = calculateTemperature(currentADCValue);
    
    Serial.print(F("ADC prekid: "));
    Serial.print(adcCount);
    Serial.print(F(" puta, Temperatura: "));
    Serial.print(currentTemp);
    Serial.println(F("°C"));
    
    // Simulacija neke obrade koja traje
    delay(50);
    
    adcReady = false;
    processingADC = false;
  }
  
  // Ažuriranje LCD-a i ispisivanje statistike svake 2 sekunde
  static unsigned long lastStatTime = 0;
  if (millis() - lastStatTime > 2000) {
    updateLCD();
    
    Serial.println(F("\n--- STATISTIKA ---"));
    Serial.print(F("Ukupno prekida - Button1: "));
    Serial.print(button1Count);
    Serial.print(F(", Button2: "));
    Serial.print(button2Count);
    Serial.print(F(", Timer: "));
    Serial.print(timerCount);
    Serial.print(F(", ADC: "));
    Serial.println(adcCount);
    
    Serial.print(F("Detektirano konflikata resursa: "));
    Serial.println(conflictCount);
    Serial.println(F("-----------------\n"));
    
    lastStatTime = millis();
  }
}