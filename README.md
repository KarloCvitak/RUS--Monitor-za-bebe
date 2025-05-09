# Razvoj ugradbenih sustava

Doxygen [generirana dokumentacija koda.](https://karlocvitak.github.io/RUS--Monitor-za-bebe/).

Detaljan opis sustava se nalazi na [Wiki stranici.](https://github.com/KarloCvitak/RUS--Monitor-za-bebe/wiki).


# Opis projekta

> Ovaj projekt je rezultat samostalnog rada u sklopu projektnog zadatka kolegija Razvoj ugradbenih sustava na Tehničkom veleučilištu u Zagrebu.Pametni Monitor za Bebe je sustav namijenjen nadzoru okoline i osnovnih parametara pomoću nekoliko senzora i bežične komunikacije. Projekt je simuliran putem Wokwi platforme, uz fokus na testiranje funkcionalnosti i optimizaciju potrošnje energije.  


# Funkcijski zahtjevi
> Mjerenje temperature i vlage okoline (DHT22/BME280).

> Detekcija zvuka pomoću senzora zvuka.

> Mjerenje vlage i temeperature zraka (DHT22).

> Alarmni sustav koji obavještava o promjenama stanja (Discord webhook).

> Energetska optimizacija kroz Light Sleep.


# Tehnologije
| Komponenta | Opis |
|------------|------|
| **Mikrokontroler** | ESP32 |
| **Senzori** | DHT22, mikrofon |
| **Bežična komunikacija** | Wi-Fi |
| **Sabirnice** | I²C |
| **Simulacija** | Wokwi |

# Instalcija

Za pokretanje ovog projekta u Wokwi simulatoru:

1. Preuzeti repozitorij (Code > Local > Download ZIP) 
2. Stvorite novi ESP32 Arduino projekt u Wokwi simulatoru
3. Učitat mapu projekta
5. Pokrenut simulaciju



## 📂 Struktura repozitorija
```
📁 RUS--Monitor-za-bebe
│── 📂 Lab1            # rješenje zadatka 1.
│── 📂 Lab2            # rješenje zadatka 2.
│── 📂 Projekt         # Implementacija projekta Monitora za bebe
│── 📄 README.md       # Ovaj dokument
```

# Članovi tima 
> Karlo Cvitak


# 📝 Licenca
Važeča (1)
[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa]

Ovaj repozitorij sadrži otvoreni obrazovni sadržaji (eng. Open Educational Resources)  i licenciran je prema pravilima Creative Commons licencije koja omogućava da preuzmete djelo, podijelite ga s drugima uz 
uvjet da navođenja autora, ne upotrebljavate ga u komercijalne svrhe te dijelite pod istim uvjetima [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License HR][cc-by-nc-sa].
>
> ### Napomena:
>
> Svi paketi distribuiraju se pod vlastitim licencama.
> Svi upotrijebleni materijali  (slike, modeli, animacije, ...) distribuiraju se pod vlastitim licencama.

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: https://creativecommons.org/licenses/by-nc/4.0/deed.hr 
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg

Orginal [![cc0-1.0][cc0-1.0-shield]][cc0-1.0]
>
>COPYING: All the content within this repository is dedicated to the public domain under the CC0 1.0 Universal (CC0 1.0) Public Domain Dedication.
>
[![CC0-1.0][cc0-1.0-image]][cc0-1.0]

[cc0-1.0]: https://creativecommons.org/licenses/by/1.0/deed.en
[cc0-1.0-image]: https://licensebuttons.net/l/by/1.0/88x31.png
[cc0-1.0-shield]: https://img.shields.io/badge/License-CC0--1.0-lightgrey.svg

### Reference na licenciranje repozitorija
