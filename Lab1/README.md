# Arduino sustav za upravljanje prekidima

Ovaj projekt demonstrira sustav upravljanja prekidima za Arduino, koji rukuje višestrukim prekidima s različitim prioritetima. Sustav efikasno upravlja različitim događajima koristeći odgovarajuće strategije rukovanja prekidima.

## Mogućnosti

- Više vrsta prekida s različitim razinama prioriteta:
  - Prekidi tipkala (najviši prioritet)
  - Prekidi timera (srednji prioritet)
  - ADC/senzor temperature prekidi (najniži prioritet)
- Detekcija i praćenje konflikta resursa
- Vizualizacija u stvarnom vremenu pomoću LED dioda i LCD zaslona
- Praćenje temperature pomoću NTC termistora
- Sveobuhvatno praćenje statistike

## Hardverski zahtjevi

- Arduino Uno
- 4 LED diode s otpornicima od 220Ω
  - Crvena LED
  - Plava LED
  - Zelena LED
  - Žuta LED
- 2 tipkala (crveno i plavo)
- NTC termistor
- LCD 1602 zaslon s I2C sučeljem

## Postavljanje sklopa u Wokwi simulatoru

### Dijelovi

1. Arduino Uno
2. 2 tipkala:
   - Crveno tipkalo (btn1)
   - Plavo tipkalo (btn2)
3. 4 LED diode:
   - Crvena LED (led1)
   - Plava LED (led2)
   - Zelena LED (led3)
   - Žuta LED (led4)
4. 4 otpornika za LED diode (220Ω)
5. NTC senzor temperature
6. LCD 1602 zaslon s I2C sučeljem

### Spajanje

#### Tipkala:
- Crveno tipkalo (btn1):
  - Jedna nožica na pin 2 Arduina
  - Druga nožica na GND Arduina
- Plavo tipkalo (btn2):
  - Jedna nožica na pin 3 Arduina
  - Druga nožica na GND Arduina

#### LED diode:
- Crvena LED (led1):
  - Anoda (A) preko otpornika od 220Ω na pin 8 Arduina
  - Katoda (C) na GND Arduina
- Plava LED (led2):
  - Anoda (A) preko otpornika od 220Ω na pin 9 Arduina
  - Katoda (C) na GND Arduina
- Zelena LED (led3):
  - Anoda (A) preko otpornika od 220Ω na pin 10 Arduina
  - Katoda (C) na GND Arduina
- Žuta LED (led4):
  - Anoda (A) preko otpornika od 220Ω na pin 11 Arduina
  - Katoda (C) na GND Arduina

#### NTC senzor temperature:
- VCC nožica na 5V Arduina
- OUT nožica na A0 Arduina
- GND nožica na GND Arduina

#### LCD 1602 I2C:
- GND na GND Arduina
- VCC na 5V Arduina
- SDA na A4 Arduina
- SCL na A5 Arduina

## Postavljanje Wokwi simulatora

Za pokretanje ovog projekta u Wokwi simulatoru:

1. Stvorite novi Arduino projekt u Wokwi simulatoru
2. Kopirajte priloženi diagram.json u projekt
3. Kopirajte priloženi Arduino kod u sketch.ino
4. Stvorite datoteku `libraries.txt` sa sljedećim sadržajem:

```
LiquidCrystal_I2C
```

5. Pokrenite simulaciju

## Opis sustava

### Prioriteti prekida (od najvišeg do najnižeg)

1. **Tipkalo 1 (INT0)** - Vanjski prekid najvišeg prioriteta
2. **Tipkalo 2 (INT1)** - Vanjski prekid srednjeg prioriteta
3. **Timer1** - Programiran da se aktivira svake sekunde
4. **ADC** - Najniži prioritet, aktivira se nakon Timer1 za mjerenje temperature

### Prekidne rutine (ISR)

- **ISR_Button1**: Obrađuje pritiske crvenog tipkala, mijenja stanje crvene LED
- **ISR_Button2**: Obrađuje pritiske plavog tipkala, mijenja stanje plave LED
- **Timer1 ISR**: Aktivira se svake sekunde, mijenja stanje zelene LED i pokreće ADC konverziju
- **ADC ISR**: Obrađuje analogno očitanje temperature i mijenja stanje žute LED

### Upravljanje konfliktima resursa

Sustav koristi zastavice za detekciju i brojanje konflikata resursa:
- `processingButton1`
- `processingButton2`
- `processingTimer`
- `processingADC`

Kada se jedan prekid dogodi dok se drugi obrađuje, brojač konflikata se povećava.

### Ugniježđeni prekidi

Sustav omogućuje ugniježđene prekide u Timer1 ISR-u koristeći funkciju `sei()`, što omogućuje prekidima višeg prioriteta da prekinu obradu timera.

### Debouncing

Unosi tipkala imaju implementiran debouncing s odgodom od 200ms kako bi se spriječila višestruka aktiviranja od jednog pritiska.

## Informacije na zaslonu

### LCD zaslon

LCD prikazuje:
- Prvi red: Brojači prekida za Tipkalo 1, Tipkalo 2 i Timer
- Drugi red: Trenutna temperatura i broj konflikata

### Serijski monitor

Serijski monitor (9600 bauda) pruža detaljne informacije:
- Detalji inicijalizacije sustava
- Pojedinačne aktivacije prekida
- Kompletna statistika svake 2 sekunde

## Kako radi

1. Tijekom postavljanja, sustav inicijalizira sve prekide s njihovim odgovarajućim prioritetima
2. Kada se dogodi prekid:
   - Odgovarajuća LED dioda mijenja stanje odmah unutar ISR-a
   - Postavlja se zastavica koja označava da prekid treba obradu
   - Brojač prekida se povećava
3. Glavna petlja provjerava i obrađuje čekajuće prekide
4. Svake 2 sekunde, statistika se prikazuje na LCD-u i serijskom monitoru

## Demonstrirane značajke

- Višestruki izvori prekida s različitim prioritetima
- Ugniježđeni prekidi (prekidi višeg prioriteta mogu prekinuti one nižeg)
- Detekcija konflikta resursa
- Efikasno rukovanje prekidima s minimalnom obradom u ISR-ima
- Interakcija s vanjskim hardverom (tipkala, LED diode, LCD, senzor temperature)
- Debouncing tipkala

## Napomene

- Izračun temperature koristi Steinhart-Hart jednadžbu za NTC termistore
- Rukovanje prekidima je dizajnirano da bude brzo, s većinom obrade koja se događa u glavnoj petlji
- Konflikti resursa se prate kako bi se demonstrirali učinci prioriteta, ali ne utječu na funkcionalnost