# Android telefonla NFC okutma (deneysel)

Firmware, SD karttaki UID eslesmesi bulunamayan ISO/IEC 14443-4 (ISO-DEP)
cihazlarda Android HCE uygulamasiyla APDU konusmayi dener. Klasik RFID
kartlarin mevcut davranisi degismez.

## Test protokolü

Telefon uygulamasinin asagidaki `SELECT AID` APDU komutuna cevap vermesi gerekir:

```
00 A4 04 00 06 F0 59 4F 4B 4C 41 00
```

Bu AID `F0YOKLA` olarak adlandirilir. Basarili cevap, ASCII olarak
`YOK:<ogrenci_no>` ve sonunda durum kelimesi `90 00` olmali:

```
59 4F 4B 3A 31 32 33 34 35 36 39 30 30 30
Y  O  K  :  1  2  3  4  5  6  9  0  0  0
```

Yukaridaki ornek, `YOK:123456` + `90 00` cevabidir. Ogrenci numarasi,
SD karttaki `ogrenciler.csv` dosyasinda kayitli olmalidir. Kabul edilen
karakterler harf, rakam, `_` ve `-`; uzunluk 2--32 karakterdir.

Google Play'deki **NFC Card Emulator** benzeri HCE test uygulamalarinda bu
komut ve cevabi bir APDU senaryosu olarak tanimlayarak donanim uyumlulugunu
deneyebilirsiniz. Telefon modeli HCE desteklemeli, NFC acik ve ekran uyanik
olmalidir.

## Guvenlik siniri

Bu sadece donanim/protokol testidir. `YOK:123456` gibi sabit bir cevap
kopyalanabilir. Gercek kullanimdan once telefon uygulamasi ile ESP32 arasina
rastgele challenge ve imzali cevap eklenmelidir. UID, Android HCE telefonlarda
her okutusta degisebildigi icin kimlik dogrulama amaciyla kullanilmaz.
