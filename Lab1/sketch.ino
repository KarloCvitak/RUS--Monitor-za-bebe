/*
 * SUSTAV S PREKIDIMA ZA ARDUINO
 * ------------------------------
 * Implementacija sustava koji koristi prekide za obradu ulaznih signala s tipkala 
 * i DHT22 senzora temperature te generira vizualne indikacije pomoću LED dioda.
 * 
 * Glavne funkcionalnosti:
 * - Rukovanje prekidima za 3 tipkala s različitim prioritetima (INT0, INT1, INT2)
 * - Generiranje tajmerskog prekida svakih 1 sekundu (TIMER1)
 * - Mjerenje temperature pomoću DHT22 digitalnog senzora
 * - Vizualna indikacija događaja kroz različite LED diode
 * - Poštivanje prioriteta prekida: TIMER1 > INT0 > INT1 > INT2 > senzor temperature
 */

// Uključivanje DHT biblioteke
#include <DHT.h>

// ======================= DEFINICIJE PINOVA =======================
// Pinovi za tipkala - svaki povezan s odgovarajućim interrupt pinom
const int BUTTON0_PIN = 2;  // INT0 - visoki prioritet
const int BUTTON1_PIN = 3;  // INT1 - srednji prioritet
const int BUTTON2_PIN = 21; // INT2 - niski prioritet (dostupan na Arduino Mega)

// Pinovi za LED diode - svaka LED signalizira odgovarajući događaj
const int LED_INT0_PIN = 4;  // LED za BUTTON0 prekid
const int LED_INT1_PIN = 5;  // LED za BUTTON1 prekid
const int LED_INT2_PIN = 6;  // LED za BUTTON2 prekid
const int LED_TIMER_PIN = 7; // LED za Timer prekid
const int LED_TEMP_PIN = 8;  // LED za temperaturni senzor

// Pin za DHT22 senzor
#define DHTPIN 9      // Digitalni pin na koji je spojen DHT22
#define DHTTYPE DHT22 // Tip DHT senzora (DHT22)

// Inicijalizacija DHT senzora
DHT dht(DHTPIN, DHTTYPE);

// ======================= STATUSNE VARIJABLE =======================
// Volatile oznaka je ključna jer se ove varijable mijenjaju unutar prekidnih rutina (ISR)
// i treba osigurati da kompajler ne optimizira pristup ovim varijablama
volatile bool led_int0_active = false; // Prati je li LED za BUTTON0 trenutno aktivna
volatile bool led_int1_active = false; // Prati je li LED za BUTTON1 trenutno aktivna
volatile bool led_int2_active = false; // Prati je li LED za BUTTON2 trenutno aktivna
volatile bool led_timer_active = false; // Prati je li LED za Timer trenutno aktivna
volatile bool led_temp_active = false; // Prati treba li LED za temperaturu treptati

// ======================= VREMENSKI MARKERI =======================
// Varijable za praćenje vremena aktivacije pojedinih događaja
// Koriste se za implementaciju neblokirajućeg rada - alternative za delay()
volatile unsigned long led_int0_start_time = 0;   // Vrijeme kada je aktivirana LED za BUTTON0
volatile unsigned long led_int1_start_time = 0;   // Vrijeme kada je aktivirana LED za BUTTON1
volatile unsigned long led_int2_start_time = 0;   // Vrijeme kada je aktivirana LED za BUTTON2
volatile unsigned long led_timer_start_time = 0;  // Vrijeme kada je aktivirana LED za Timer
volatile unsigned long last_temp_check_time = 0;  // Zadnje vrijeme provjere temperature
volatile unsigned long last_temp_toggle_time = 0; // Zadnje vrijeme promjene stanja LED temperature

// ======================= DEBOUNCE MEHANIZAM =======================
// Varijable za implementaciju debounce mehanizma za tipkala
// Sprječavaju višestruko aktiviranje prekida zbog mehaničkog odbijanja kontakata
volatile unsigned long last_button0_time = 0; // Zadnje vrijeme pritiska BUTTON0
volatile unsigned long last_button1_time = 0; // Zadnje vrijeme pritiska BUTTON1
volatile unsigned long last_button2_time = 0; // Zadnje vrijeme pritiska BUTTON2
const unsigned long DEBOUNCE_DELAY = 200;     // Minimalno vrijeme između dva pritiska (200ms)

// ======================= KONSTANTE ZA TRAJANJA =======================
// Definicije trajanja za različite događaje i intervale
const unsigned long LED_BLINK_DURATION = 1000;    // Trajanje svijetljenja LED dioda nakon pritiska tipke (1s)
const unsigned long TIMER_LED_DURATION = 100;     // Trajanje svijetljenja Timer LED diode (100ms)
const unsigned long TEMP_CHECK_INTERVAL = 2000;   // Interval provjere temperature (2s za DHT22)
const unsigned long TEMP_BLINK_INTERVAL = 200;    // Interval treptanja LED diode temperature (200ms)
const float TEMP_THRESHOLD = 25.0;                // Prag temperature za aktiviranje upozorenja (25°C)

// ======================= MJERENJE TEMPERATURE =======================
// Varijable za pohranu izmjerenih vrijednosti
volatile float temperature = 0.0; // Trenutno izmjerena temperatura u °C
volatile float humidity = 0.0;    // Trenutno izmjerena vlažnost zraka u %

// ======================= INTERRUPT SERVICE RUTINE (ISR) =======================

/*
 * ISR za BUTTON0 (INT0) - visoki prioritet prekida
 * Aktivira LED_INT0 na 1 sekundu prilikom pritiska tipkala
 * Implementira debounce mehanizam da spriječi višestruka aktiviranja
 */
void button0_ISR() {
  unsigned long current_time = millis();
  // Provjera je li prošlo dovoljno vremena od zadnjeg pritiska (debounce)
  if (current_time - last_button0_time > DEBOUNCE_DELAY) {
    last_button0_time = current_time;  // Ažuriranje vremena zadnjeg pritiska
    led_int0_active = true;            // Označavanje da je LED aktivna
    led_int0_start_time = current_time; // Bilježenje vremena aktivacije
    digitalWrite(LED_INT0_PIN, HIGH);   // Uključivanje LED diode
  }
}

/*
 * ISR za BUTTON1 (INT1) - srednji prioritet prekida
 * Aktivira LED_INT1 na 1 sekundu prilikom pritiska tipkala
 * Implementira debounce mehanizam da spriječi višestruka aktiviranja
 */
void button1_ISR() {
  unsigned long current_time = millis();
  if (current_time - last_button1_time > DEBOUNCE_DELAY) {
    last_button1_time = current_time;
    led_int1_active = true;
    led_int1_start_time = current_time;
    digitalWrite(LED_INT1_PIN, HIGH);
  }
}

/*
 * ISR za BUTTON2 (INT2) - niski prioritet prekida
 * Aktivira LED_INT2 na 1 sekundu prilikom pritiska tipkala
 * Implementira debounce mehanizam da spriječi višestruka aktiviranja
 */
void button2_ISR() {
  unsigned long current_time = millis();
  if (current_time - last_button2_time > DEBOUNCE_DELAY) {
    last_button2_time = current_time;
    led_int2_active = true;
    led_int2_start_time = current_time;
    digitalWrite(LED_INT2_PIN, HIGH);
  }
}

/*
 * ISR za Timer1 prekid - najviši prioritet
 * Aktivira LED_Timer na kratko (100ms) svake sekunde
 * Poziva se automatski kada Timer1 dostigne konfigurirani compare match
 */
void timer1_ISR() {
  unsigned long current_time = millis();
  led_timer_active = true;
  led_timer_start_time = current_time;
  digitalWrite(LED_TIMER_PIN, HIGH);
}

/*
 * Funkcija za inicijalizaciju Timer1
 * Konfigurira Timer1 da generira prekid svake 1 sekunde
 * Koristi CTC (Clear Timer on Compare Match) mode
 */
void setupTimer1() {
  cli(); // Isključi prekide tijekom konfiguracije (Critical Section)
  
  // Resetiranje registara Timer1
  TCCR1A = 0; // Timer/Counter Control Register A
  TCCR1B = 0; // Timer/Counter Control Register B
  TCNT1 = 0;  // Timer/Counter početna vrijednost
  
  // Postavi Compare Match Register (OCR1A) za 1Hz frekvenciju prekida
  // Formula: OCR1A = (CPU_freq / (prescaler * desired_freq)) - 1
  // Za 16MHz CPU, prescaler 1024 i 1Hz: OCR1A = (16MHz / (1024 * 1Hz)) - 1 = 15624
  OCR1A = 15624;
  
  // Uključi CTC mode (WGM12) - broji do vrijednosti OCR1A i resetira se
  TCCR1B |= (1 << WGM12);
  
  // Postavi prescaler na 1024 (CS12 i CS10 bitovi)
  TCCR1B |= (1 << CS12) | (1 << CS10);
  
  // Omogući Timer Compare A prekid
  TIMSK1 |= (1 << OCIE1A);
  
  sei(); // Ponovno uključi prekide
}

/*
 * Funkcija za čitanje temperature s DHT22 senzora
 * Vraća true ako je očitanje uspješno, false ako nije
 */
bool readDHT22() {
  // Čitanje temperature i vlažnosti
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  
  // Provjera jesu li očitanja valjana (nisu NaN)
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Greška u čitanju s DHT22 senzora!");
    return false;
  }
  
  return true;
}

/*
 * Setup funkcija - inicijalizacija sustava
 * Postavlja konfiguraciju pinova, prekida i početnih stanja
 */
void setup() {
  // Inicijalizacija serijske komunikacije za debugging
  Serial.begin(9600);
  
  // Inicijalizacija DHT senzora
  dht.begin();
  
  // Konfiguracija pinova
  pinMode(BUTTON0_PIN, INPUT_PULLUP);  // Pullup otpornici aktiviraju prekid kad se pin spoji na GND
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(LED_INT0_PIN, OUTPUT);
  pinMode(LED_INT1_PIN, OUTPUT);
  pinMode(LED_INT2_PIN, OUTPUT);
  pinMode(LED_TIMER_PIN, OUTPUT);
  pinMode(LED_TEMP_PIN, OUTPUT);
  
  // Postavi početna stanja LED dioda na isključeno
  digitalWrite(LED_INT0_PIN, LOW);
  digitalWrite(LED_INT1_PIN, LOW);
  digitalWrite(LED_INT2_PIN, LOW);
  digitalWrite(LED_TIMER_PIN, LOW);
  digitalWrite(LED_TEMP_PIN, LOW);
  
  // Konfiguracija prekida za tipkala
  // FALLING detekcija - prekid se aktivira kad pin prijeđe iz HIGH u LOW
  // Ovo se događa kad se tipkalo pritisne jer su pinovi konfigurirani s pullup otpornicima
  attachInterrupt(digitalPinToInterrupt(BUTTON0_PIN), button0_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON1_PIN), button1_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON2_PIN), button2_ISR, FALLING);
  
  // Konfiguracija Timer1 prekida za generiranje prekida svake 1s
  setupTimer1();
  
  Serial.println("Sustav inicijaliziran!");
  Serial.println("DHT22 senzor temperature i vlažnosti spojen na pin 9");
  Serial.println("LED će treptati kada temperatura padne ispod 25°C");
}

/*
 * Glavna petlja (loop) - kontinuirano izvršavanje programa
 * Upravlja LED diodama i provjerava temperaturu
 * Koristi neblokirajuće tehnike s millis() umjesto delay()
 */
void loop() {
  // Dohvati trenutno vrijeme za upravljanje vremenski ovisnim događajima
  unsigned long current_time = millis();
  
  // -------- Upravljanje LED_INT0 (BUTTON0) --------
  if (led_int0_active && (current_time - led_int0_start_time >= LED_BLINK_DURATION)) {
    led_int0_active = false;              // LED više nije aktivna
    digitalWrite(LED_INT0_PIN, LOW);      // Isključi LED
    Serial.println("LED_INT0 isključena nakon 1s");
  }
  
  // -------- Upravljanje LED_INT1 (BUTTON1) --------
  if (led_int1_active && (current_time - led_int1_start_time >= LED_BLINK_DURATION)) {
    led_int1_active = false;
    digitalWrite(LED_INT1_PIN, LOW);
    Serial.println("LED_INT1 isključena nakon 1s");
  }
  
  // -------- Upravljanje LED_INT2 (BUTTON2) --------
  if (led_int2_active && (current_time - led_int2_start_time >= LED_BLINK_DURATION)) {
    led_int2_active = false;
    digitalWrite(LED_INT2_PIN, LOW);
    Serial.println("LED_INT2 isključena nakon 1s");
  }
  
  // -------- Upravljanje LED_Timer (TIMER1) --------
  if (led_timer_active && (current_time - led_timer_start_time >= TIMER_LED_DURATION)) {
    led_timer_active = false;
    digitalWrite(LED_TIMER_PIN, LOW);
    Serial.println("Timer prekid aktiviran");
  }
  
  // -------- Mjerenje temperature s DHT22 i upravljanje LED_Temp --------
  // Periodično mjerenje temperature (svakih 2000ms) - najmanjeg prioriteta
  // DHT22 ne bi trebalo očitavati češće od 2 sekunde
  if (current_time - last_temp_check_time >= TEMP_CHECK_INTERVAL) {
    last_temp_check_time = current_time;
    
    // Očitaj temperaturu i vlažnost
    if (readDHT22()) {
      // Provjeri je li temperatura ispod praga
      if (temperature < TEMP_THRESHOLD) {
        // Temperatura je ispod praga - aktiviraj treptanje LED
        led_temp_active = true;
        Serial.print("Niska temperatura detektirana: ");
        Serial.print(temperature);
        Serial.print(" °C, Vlažnost: ");
        Serial.print(humidity);
        Serial.println(" %");
      } else {
        // Temperatura je iznad praga - isključi LED
        led_temp_active = false;
        digitalWrite(LED_TEMP_PIN, LOW);
        
        Serial.print("Temperatura: ");
        Serial.print(temperature);
        Serial.print(" °C, Vlažnost: ");
        Serial.print(humidity);
        Serial.println(" %");
      }
    }
  }
  
  // -------- Upravljanje treptanjem LED_Temp --------
  // Ako je temperatura niska, LED treperi svakih 200ms
  if (led_temp_active) {
    if (current_time - last_temp_toggle_time >= TEMP_BLINK_INTERVAL) {
      last_temp_toggle_time = current_time;
      // Toggle LED stanje (ON->OFF ili OFF->ON)
      digitalWrite(LED_TEMP_PIN, !digitalRead(LED_TEMP_PIN));
    }
  }
}

/*
 * Timer1 Compare Match A Interrupt Service Routine
 * Automatski se poziva kad Timer1 dostigne vrijednost OCR1A
 * Interno povezan s timer1_ISR() funkcijom
 */
ISR(TIMER1_COMPA_vect) {
  timer1_ISR();
}