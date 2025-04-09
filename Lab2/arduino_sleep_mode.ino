/**
 * @file arduino_sleep_mode.ino
 * @brief Program za upravljanje potrošnjom energije Arduino mikrokontrolera korištenjem sleep modova
 * @author Karlo
 * @date 2025-04-09
 * 
 * @details Ovaj program demonstrira razne tehnike za smanjenje potrošnje energije
 * na Arduino platformi korištenjem sleep modova. Implementira različite mehanizme
 * buđenja, uključujući vanjski prekid (tipkalo) i timer prekid (watchdog timer).
 */

 #include <avr/sleep.h>
 #include <avr/power.h>
 #include <avr/wdt.h>
 
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
  * @brief Zastavica koja označava buđenje putem tipkala
  */
 volatile bool wakeUpByButton = false;
 
 /**
  * @brief Zastavica koja označava buđenje putem watchdog timera
  */
 volatile bool wakeUpByTimer = false;
 
 /**
  * @brief Trenutno odabrani sleep mode
  * @see sleep.h za dostupne modove
  */
 byte sleepMode = SLEEP_MODE_PWR_DOWN;
 
 // Prototipi funkcija
 void setupExternalInterrupt();
 void enterSleep();
 void setupWatchdog(int timerPrescaler);
 void changeMode();
 
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
  * 2. Ako je pritisnuta tipka, mijenja sleep mode
  * 3. Konfigurira watchdog timer na 8 sekundi
  * 4. Ulazi u odabrani sleep mode
  * 5. Nakon buđenja, ispisuje što je uzrokovalo buđenje
  */
 void loop() {
   // Aktivan period - uključi LED na 5 sekundi
   Serial.println(F("Uključujem LED na 5 sekundi..."));
   power_timer0_enable();  // Enable Timer0 za millis()
   digitalWrite(LED_PIN, HIGH);
   delay(5000);
   digitalWrite(LED_PIN, LOW);
   
   // Ako je pritisnuta tipka, promijeni sleep mode
   if (wakeUpByButton) {
     changeMode();
     wakeUpByButton = false;
   }
   
   // Priprema za sleep mode
   Serial.println(F("Ulazim u sleep mode..."));
   Serial.println(F("Buđenje će biti nakon 8 sekundi ili pritiskom tipke"));
   Serial.flush();  // Osigurati da su svi podaci poslani prije spavanja
   
   // Postavljanje watchdog timera na 8 sekundi
   setupWatchdog(9);  // 9 = 8 sekundi
   
   // Ulazak u sleep mode
   enterSleep();
   
   // Kod nastavlja ovdje nakon buđenja
   if (wakeUpByTimer) {
     Serial.println(F("Buđenje uzrokovano timerom!"));
     wakeUpByTimer = false;
   } else if (wakeUpByButton) {
     Serial.println(F("Buđenje uzrokovano pritiskom tipke!"));
   }
   
   delay(1000);  // Mali delay za stabilizaciju
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
  * Postavi zastavicu i onemogući sleep mode kada se pritisne tipkalo.
  */
 void buttonInterrupt() {
   wakeUpByButton = true;
   sleep_disable();  // Odmah onemogući sleep mode
 }
 
 /**
  * @brief Interrupt Service Routine (ISR) za Watchdog Timer
  * 
  * Postavi zastavicu i onemogući watchdog timer kada istekne vrijeme timera.
  */
 ISR(WDT_vect) {
   wakeUpByTimer = true;
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
  * stavlja mikrokontroler u odabrani sleep mode.
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
   
   set_sleep_mode(sleepMode);  // Postavi odabrani sleep mode
   
   sleep_enable();             // Omogući sleep mode
   
   // Samo kod PWR_DOWN i STANDBY modova, omogući BOD disable za minimalnu potrošnju
   if (sleepMode == SLEEP_MODE_PWR_DOWN || sleepMode == SLEEP_MODE_STANDBY) {
     // Posebna sekvenca za isključivanje BOD-a tijekom sleep moda (samo za AVR s BOD disable bit-om)
     #if defined(BODS) && defined(BODSE)
       sleep_bod_disable();
     #endif
   }
   
   // Ulazak u sleep mode
   sleep_mode();
   
   // Program nastavlja ovdje nakon buđenja
   sleep_disable();  // Onemogući sleep odmah nakon buđenja
   
   // Ponovno omogući potrebne periferije
   power_all_enable();  // Uključi sve periferije
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
   
   Serial.println(F("Usporedba potrošnje energije:"));
   Serial.println(F("PWR_DOWN: Najniža potrošnja (~0.1μA), sve periferije isključene"));
   Serial.println(F("PWR_SAVE: Niska potrošnja (~1μA), samo Timer2 aktivan"));
   Serial.println(F("STANDBY: Slično PWR_DOWN, ali glavni oscilator radi (~0.8μA)"));
   Serial.println(F("IDLE: Viša potrošnja (~4-15mA), CPU zaustavljen, većina periferija aktivna"));
 }
