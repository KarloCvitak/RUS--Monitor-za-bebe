/**
 * @file arduino_sleep_mode.ino
 * @brief Program za upravljanje potrošnjom energije Arduino mikrokontrolera korištenjem sleep modova
 * @author kcvitak
 * @date 2025-04-10
 * 
 * @details Ovaj program demonstrira razne tehnike za smanjenje potrošnje energije
 * na Arduino platformi korištenjem sleep modova. Implementira različite mehanizme
 * buđenja, uključujući vanjski prekid (tipkalo) i timer prekid (watchdog timer).
 */

 #include <avr/sleep.h>
 #include <avr/power.h>
 #include <avr/wdt.h>
 #include <avr/interrupt.h>
 
 /**
  * @brief Pin na kojem je spojena LED dioda
  */
 const int LED_PIN = 13;
 
 /**
  * @brief Pin na kojem je spojeno tipkalo za vanjski prekid
  * @note Mora biti pin 2 (INT0) ili pin 3 (INT1) na standardnom Arduinu
  */
 const int BUTTON_PIN = 2;
 
 /**
  * @brief Pin za debugging sleep modova u Wokwi
  */
 const int DEBUG_PIN = 4;
 
 /**
  * @brief Zastavica koja označava stanje spavanja
  */
 volatile bool isSleeping = true;
 
 /**
  * @brief Zastavica koja označava buđenje putem tipkala
  */
 volatile bool buttonWakeUp = false;
 
 /**
  * @brief Trenutno odabrani sleep mode
  * @see sleep.h za dostupne modove
  */
 byte sleepMode = SLEEP_MODE_PWR_DOWN;
 
 /**
  * @brief Za praćenje zadnjeg vremena za debounce tipkala
  */
 volatile unsigned long lastButtonPress = 0;
 
 // Prototipi funkcija
 void setupExternalInterrupt();
 void enterSleep();
 void setupWatchdog(int timerPrescaler);
 void changeMode();
 void disableWatchdog();
 
 /**
  * @brief Inicijalizacija programa
  * 
  * Postavlja pinove, konfigurira prekide i isključuje nekorištene periferije
  * za smanjenje potrošnje energije.
  */
 void setup() {
   Serial.begin(9600);
   
   // Postavljanje pinova
   pinMode(LED_PIN, OUTPUT);
   pinMode(BUTTON_PIN, INPUT_PULLUP);
   pinMode(DEBUG_PIN, OUTPUT);
   
   // Osiguraj da je watchdog timer isključen na početku
   disableWatchdog();
   
   // Postavljanje prekida
   setupExternalInterrupt();
   
   // Inicijalno isključivanje nekorištenih periferija za štednju energije
   power_adc_disable();  // Isključi ADC
   power_spi_disable();  // Isključi SPI
   power_twi_disable();  // Isključi TWI/I2C
   
   Serial.println(F("Arduino Sleep Mode Demonstracija"));
   Serial.println(F("-----------------------------"));
   Serial.println(F("Trenutni mod: SLEEP_MODE_PWR_DOWN"));
   Serial.println(F("Pritisnite tipku za buđenje i promjenu sleep moda"));
 }
 
 /**
  * @brief Glavna petlja programa
  * 
  * Izvršava sljedeći slijed operacija:
  * 1. Uključuje LED na 5 sekundi (aktivan period)
  * 2. Ako je probuđen pritiskom na tipku, mijenja sleep mode
  * 3. Konfigurira watchdog timer na 8 sekundi
  * 4. Ulazi u odabrani sleep mode
  * 5. Nakon buđenja, ispisuje što je uzrokovalo buđenje
  */
 void loop() {
   // Aktivan period - uključi LED na 5 sekundi
   Serial.println(F("Uključujem LED na 5 sekundi..."));
   digitalWrite(LED_PIN, HIGH);
   delay(5000);
   digitalWrite(LED_PIN, LOW);
   
   // Debug signal za Wokwi - pokazuje aktivni period
   digitalWrite(DEBUG_PIN, HIGH);
   delay(200);
   digitalWrite(DEBUG_PIN, LOW);
   
   Serial.println(F("Ulazim u sleep mode..."));
   Serial.println(F("Buđenje će biti nakon 8 sekundi ili pritiskom tipke"));
   Serial.flush();  // Osigurati da su svi podaci poslani prije spavanja
   
   // Postavljanje watchdog timera na 8 sekundi
   setupWatchdog(9);  // 9 = 8 sekundi
   
   // Indikacija za Wokwi - uzorak bljeskanja za trenutni mod
   visualizeSleepMode();
   
   // Ulazak u sleep mode
   enterSleep();
   
   // Kod nastavlja ovdje nakon buđenja
   Serial.println(F("Probudili smo se!"));
   
   // Provjeri zastavicu za buđenje tipkom
   if (buttonWakeUp) {
     Serial.println(F("Buđenje uzrokovano pritiskom tipke!"));
     changeMode();  // Promijeni mod kada smo probuđeni tipkom
     buttonWakeUp = false;  // Resetiraj zastavicu
   } else {
     Serial.println(F("Buđenje uzrokovano timerom!"));
   }
   
   delay(1000);  // Mali delay za stabilizaciju
   isSleeping = true;  // Resetiramo zastavicu za sljedeći ciklus
 }
 
 /**
  * @brief Vizualizira trenutni sleep mod kroz LED bljeskanje
  * 
  * Za Wokwi simulaciju, ova funkcija pokazuje koji je trenutni
  * sleep mod aktiviran različitim uzorkom bljeskanja.
  */
 void visualizeSleepMode() {
   switch (sleepMode) {
     case SLEEP_MODE_PWR_DOWN:
       // Jedan brzi bljesak za PWR_DOWN
       digitalWrite(DEBUG_PIN, HIGH);
       delay(100);
       digitalWrite(DEBUG_PIN, LOW);
       break;
     case SLEEP_MODE_PWR_SAVE:
       // Dva brza bljeska za PWR_SAVE
       for (int i = 0; i < 2; i++) {
         digitalWrite(DEBUG_PIN, HIGH);
         delay(100);
         digitalWrite(DEBUG_PIN, LOW);
         delay(100);
       }
       break;
     case SLEEP_MODE_STANDBY:
       // Tri brza bljeska za STANDBY
       for (int i = 0; i < 3; i++) {
         digitalWrite(DEBUG_PIN, HIGH);
         delay(100);
         digitalWrite(DEBUG_PIN, LOW);
         delay(100);
       }
       break;
     case SLEEP_MODE_IDLE:
       // Dugački bljesak za IDLE
       digitalWrite(DEBUG_PIN, HIGH);
       delay(500);
       digitalWrite(DEBUG_PIN, LOW);
       break;
   }
 }
 
 /**
  * @brief Isključuje watchdog timer
  * 
  * Sigurnosna funkcija za potpuno isključivanje watchdog timera
  */
 void disableWatchdog() {
   // Clearing the WDT reset flag
   MCUSR &= ~(1<<WDRF);
   
   // Disable the WDT
   wdt_disable();
 }
 
 /**
  * @brief Postavlja vanjski prekid na pin za tipkalo
  * 
  * Konfigurira vanjski prekid za buđenje mikrokontrolera na padajući brid
  * signala (pritisak tipke).
  */
 void setupExternalInterrupt() {
   attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInterrupt, FALLING);
 }
 
 /**
  * @brief Interrupt Service Routine (ISR) za tipkalo
  * 
  * Postavlja zastavice za buđenje iz sleep moda i označava buđenje tipkom
  */
 void buttonInterrupt() {
   // Debounce mehanizam
   unsigned long currentMillis = millis();
   if (currentMillis - lastButtonPress > 200) {  // 200ms debounce period
     isSleeping = false;
     buttonWakeUp = true;
     lastButtonPress = currentMillis;
   }
 }
 
 /**
  * @brief Interrupt Service Routine (ISR) za Watchdog Timer
  * 
  * Postavlja zastavicu i onemogući watchdog timer kada istekne vrijeme timera.
  */
 ISR(WDT_vect) {
   isSleeping = false;
   wdt_disable();  // Onemogući watchdog timer
 }
 
 /**
  * @brief Postavlja watchdog timer za buđenje nakon određenog vremena
  * 
  * @param timerPrescaler Vrijednost prescalera koja određuje vrijeme:
  *        0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms,
  *        6=1s, 7=2s, 8=4s, 9=8s
  */
 void setupWatchdog(int timerPrescaler) {
   // Resetiranje watchdog timera
   wdt_reset();
   
   // Postavljanje WDT registara za omogućavanje prekida
   byte wdtBits = timerPrescaler & 7;
   if (timerPrescaler > 7) {
     wdtBits |= (1 << 5);  // Postavi WDP3 bit
   }
   
   // Posebna sekvenca za promjenu registara
   MCUSR &= ~(1 << WDRF);  // Očisti WDRF flag
   WDTCSR |= (1 << WDCE) | (1 << WDE);  // Omogući promjenu
   WDTCSR = (1 << WDIE) | wdtBits;      // Postavi novi timeout i WDT interrupt enable bit
 }
 
 /**
  * @brief Ulazak u odabrani sleep mode
  * 
  * Isključuje nekorištene periferije, konfigurira registre i
  * stavlja mikrokontroler u odabrani sleep mode. Čeka dok se
  * ne dogodi vanjski prekid ili istekne watchdog timer.
  */
 void enterSleep() {
   // Isključi periferije koje se ne koriste u sleep modu
   power_adc_disable();
   power_spi_disable();
   power_twi_disable();
   
   // Samo u IDLE modu, Timer0 i USART mogu ostati aktivni
   if (sleepMode != SLEEP_MODE_IDLE) {
     power_timer0_disable();
     power_usart0_disable();
   }
   
   noInterrupts();        // Onemogući prekide prije ulaska u sleep
   set_sleep_mode(sleepMode);  // Postavi odabrani sleep mode
   sleep_enable();        // Omogući sleep mode
   
   // Samo kod PWR_DOWN i STANDBY modova, omogući BOD disable za minimalnu potrošnju
   if (sleepMode == SLEEP_MODE_PWR_DOWN || sleepMode == SLEEP_MODE_STANDBY) {
     #if defined(BODS) && defined(BODSE)
       sleep_bod_disable();
     #endif
   }
   
   interrupts();          // Ponovno omogući prekide
   sleep_cpu();           // Ulazak u sleep mode
   
   // Program nastavlja ovdje nakon prvog buđenja
   sleep_disable();       // Odmah onemogući sleep
   
   // Ostani u petlji dok se ne dogodi prekid
   while (isSleeping) {}
   
   // Ponovno omogući potrebne periferije
   power_all_enable();    // Uključi sve periferije
 }
 
 /**
  * @brief Mijenja trenutni sleep mode
  * 
  * Ciklički izmjenjuje sleep modove i ispisuje informacije
  * o njihovoj potrošnji energije.
  */
 void changeMode() {
   // Ciklički izmijeni sleep modove
   switch (sleepMode) {
     case SLEEP_MODE_PWR_DOWN:
       sleepMode = SLEEP_MODE_PWR_SAVE;
       Serial.println(F("Novi mod: SLEEP_MODE_PWR_SAVE"));
       break;
     case SLEEP_MODE_PWR_SAVE:
       sleepMode = SLEEP_MODE_STANDBY;
       Serial.println(F("Novi mod: SLEEP_MODE_STANDBY"));
       break;
     case SLEEP_MODE_STANDBY:
       sleepMode = SLEEP_MODE_IDLE;
       Serial.println(F("Novi mod: SLEEP_MODE_IDLE"));
       break;
     case SLEEP_MODE_IDLE:
       sleepMode = SLEEP_MODE_PWR_DOWN;
       Serial.println(F("Novi mod: SLEEP_MODE_PWR_DOWN"));
       break;
     default:
       sleepMode = SLEEP_MODE_PWR_DOWN;
       Serial.println(F("Reset na: SLEEP_MODE_PWR_DOWN"));
       break;
   }
 }
