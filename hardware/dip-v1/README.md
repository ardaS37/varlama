# Yoklama — doğrudan lehimlenen modüller için DIP/THT kart, v1

Bu sürüm, hazır ESP32 geliştirme kartı ve modüllerin erkek pinlerini ana PCB'nin deliklerine doğrudan lehimlemek içindir. Ana kart üzerinde SMD montaj yoktur; modüllerin kendi üzerlerindeki SMD parçalar hazır modülün parçasıdır. Soket gerekmez.

**Durum: elektriksel ve PCB kural kontrolleri tamamlanmış prototip tasarım. Fiziksel montaj, USB akım bütçesi ve çalışır cihaz testi yapılmadı.** Gerberlerin bulunması bu testlerin yapıldığı anlamına gelmez.

## Dosyalar

- `yoklama-dip-v1.kicad_pro`: KiCad 10 projesi.
- `yoklama-dip-v1.kicad_sch`: bağlantı şeması; modül ve destek bileşenleri pin adlarıyla gösterilir.
- `yoklama-dip-v1.kicad_pcb`: yolları çizilmiş iki katmanlı PCB.
- `Yoklama.pretty`, `Yoklama.kicad_sym`, `fp-lib-table`, `sym-lib-table`: proje içi kütüphaneler; taşınırken birlikte tutulmalı.
- `BOM.csv`: parça listesi; `pinout.csv`: firmware ile eşleştirilmiş bağlantılar.
- `exports/assembly.svg`: üst yüzün gerçek KiCad montaj/baskı çizimi.
- `exports/board.svg`: iki bakır katman ve üst baskı.
- `exports/yoklama-dip-v1.svg`: şema önizlemesi.
- `gerbers/`: 7 Gerber katmanı, PTH ve NPTH delik dosyaları ve Gerber iş dosyası.
- `reports/erc.rpt`, `reports/drc.rpt`: son KiCad kontrolleri.
- `reports/verification.json`: kod, şema ve PCB bağlantılarının karşılaştırması.

## Kart ve besleme

| Özellik | Değer |
|---|---|
| Ana PCB | 152,4 × 127 mm, dikdörtgen |
| Katman | 2 bakır katman |
| Önerilen malzeme | FR-4, 1,6 mm, 1 oz bakır |
| Sinyal yolları | 0,30 mm |
| Güç/toprak yolları | 0,80 mm, iki yüzde GND dolgu |
| Minimum açıklık | 0,25 mm |
| Modül pin delikleri | 1,00 mm bitmiş PTH; 1,80 mm pad |
| Modül pin adımı | 2,54 mm |
| ESP32 sıra aralığı | 25,40 mm, 2 × 19 pin |
| Montaj delikleri | 4 × 3,20 mm NPTH, M3 |
| Besleme | Yalnızca ESP32 geliştirme kartının USB girişi |

USB'den gelen 5 V, geliştirme kartının 5V pininden SD modülüne, LCD'ye ve buzzer devresine dağıtılır. RC522 ve I²C'nin ESP tarafı geliştirme kartının 3V3 çıkışını kullanır. Ana kartta ayrı adaptör girişi yoktur. USB kaynağının ve geliştirme kartının 5 V yolunun toplam yüke uygunluğu ilk cihazda ölçülmelidir; bilgisayar USB portundan sınırsız akım varsayılmadı.

## Modül yerleşimi ve yön

Tüm yönler **üstten, modüllerin takıldığı yüzden görünüş** içindir. Alt yüz bakırını elle aynalamayın; üretim dosyaları doğru katman yönleriyle dışa aktarıldı.

- **U1:** ESP32-DevKitC V4 / WROOM, 38 pin, anten üstte ve USB aşağıda. Solda üst pin 3V3, sol alt pin 5V; sağ üst pin GND. `SD0/1/2/3`, `CMD`, `CLK` gibi flash pinleri bağlanmadı. WROVER, S3 veya 30 pinli kart için uygun değildir.
- **J1:** 8 pinli RC522, soldan sağa `SDA/SS, SCK, MOSI, MISO, IRQ, GND, RST, 3V3`. IRQ boş bırakıldı. Modül gövdesi pin sırasının üstüne doğru uzanır. Yer ayrılan zarf yaklaşık 40 × 60 mm; modül montaj delikleri için tahminle delik açılmadı.
- **J2:** Kullanıcının fotoğrafındaki 42 × 24 mm, regülatör ve seviye tamponu taşıyan microSD modülü. Üstten alta `GND, VCC, MISO, MOSI, SCK, CS`; gövde pinlerin sağına uzanır. VCC=5 V. Bu bağlantı çıplak microSD soketi veya regülatörsüz 3,3 V kart için değildir.
- **J3:** Klasik PCF8574 I²C LCD, soldan sağa `GND, VCC, SDA, SCL`. 4 pin doğrudan lehim bağlantısıdır. LCD kenarda dik montaj için düşünülmüştür; mevcut açılı pinleri hizalanmalıdır. LCD'nin ekran gövdesi, vida aralıkları ve kutusu bu revizyonda ölçülendirilmedi; 4 pin bağlantısı LCD için tek başına mekanik taşıyıcı sayılmamalıdır.

Üretimden önce `assembly.svg` gerçek boyutta (%100, sayfaya sığdır kapalı) basılıp modüllerin pinleriyle karşılaştırılmalıdır. Kullanıcının kabul ettiği standart ESP32 ayak izi esas alındı; fiziksel kart ölçülmedi. Sağ açılı pinleri olan modüller için pin yönü fiziksel montaj sırasında dikkate alınmalıdır.

ESP32 ve RC522 antenlerinin altında her iki bakır katmanda yol, via, pad ve dolgu yasak alanları bulunur. Bu bölgelere metal bağlantı elemanı eklenmemelidir.

## Firmware bağlantıları

| Modül | Sinyal | GPIO |
|---|---|---:|
| RC522 | SDA/SS | 5 |
| RC522 | SCK | 18 |
| RC522 | MISO | 19 |
| RC522 | MOSI | 23 |
| RC522 | RST | 27 |
| SD | CS | 33 |
| SD | SCK | 14 |
| SD | MISO | 25 |
| SD | MOSI | 26 |
| LCD | SDA, 3,3 V tarafı | 21 |
| LCD | SCL, 3,3 V tarafı | 22 |
| Hazır LED | Direnç üzerinden | 4 |
| Başarılı işlem LED | Direnç üzerinden | 16 |
| Hata LED | Direnç üzerinden | 17 |
| Ağ LED | Direnç üzerinden | 13 |
| Buzzer | Sürücü üzerinden | 32 |

Kod değiştirilmedi. `src/main.cpp` içindeki sabitler esas alındı. RC522 için `SPI.begin()` / `esp32dev` varsayılan VSPI pinleri kullanılır. SD ayrı HSPI pinlerindedir. Eski kök README'deki SD pinlerini kullanmayın. Firmware LCD'yi `16×2`, I²C adresini `0x3F` olarak başlatır; `0x27` adresli bir LCD'de adresi değiştirmek gerekir.

## Destek parçaları ve polarite

- R1–R4: 1 kΩ, 1/4 W; D1–D4: 5 mm LED, 2,54 mm bacak aralığı. LED pad 1=K/katot, pad 2=A/anot. Kare pad katottur. Mavi LED'in parlaklığı ileri gerilimine bağlı olarak düşük olabilir.
- Q1, Q2: **2N7000**, TO-92; pad 1=source, 2=gate, 3=drain. Bacaklar 2,54 mm aralığa açılır. Gate 3V3'e, source ESP32 tarafına, drain LCD'nin 5 V tarafına bağlıdır. R5–R8=4,7 kΩ çekme dirençleri. LCD sırt kartındaki çekme dirençleri 5 V tarafında kalır. Bu dönüştürücü prototipte 100 kHz I²C ile test edilmelidir.
- Q3: **BC337-40**, TO-92; pad 1=collector, 2=base, 3=emitter. R9=1 kΩ, R10=100 kΩ.
- BZ1: **pasif piezo**, 5 V, en fazla 12 mm gövde, 7,62 mm bacak aralığı, en fazla 20 mA. Pad 1 pozitif. Aktif buzzerla firmware'in farklı frekanslı `tone()` sesleri elde edilmez. R11=10 kΩ deşarj direnci; D5=1N4148, pad 1 katot/bant, pad 2 anot.
- C1=100 µF/10 V, C4=10 µF/10 V; 2,54 mm bacak aralığı ve en fazla 5 mm gövde. Kare pad 1 pozitif, pad 2 GND. C2/C3/C5=100 nF seramik, 2,54 mm bacak aralığı, polaritesiz.
- R12/R13=10 kΩ; RC522 ve SD seçme hatlarını açılışta 3V3'e çeker.

Destek parçaları için belirtilen kılıf, bacak sırası ve gövde ölçüsü BOM'un parçasıdır; aynı isimli farklı üretici/kılıf otomatik olarak eşdeğer kabul edilmemelidir.

## Kontrol kapsamı

KiCad ERC, bakır aralıkları, bağlantısız padler ve şema-PCB eşleşmesi kontrol edildi. GPIO eşlemesi ayrıca mevcut firmware sabitleriyle karşılaştırıldı. RF performansı, SPI sinyal bütünlüğü, SD yazma sırasında besleme düşümü, LCD'nin gerçek adresi ve mekanik montaj ancak ilk fiziksel prototiple doğrulanabilir. Bu revizyon için üretim siparişi verilmedi.

## Yeniden üretme

KiCad 10 ile gelen Python'da `pcbnew` ve `numpy` kullanılır. `build_design.py` PCB'yi sıfırdan oluşturur ve elle yapılan değişiklikleri üzerine yazar; elle düzenlemeden önce kopya alın.

1. `build_design.py` çalıştırılır.
2. KiCad CLI ile şema netlisti `reports/schematic.net` dosyasına aktarılır.
3. `route_board.py`, sonra `finish_board.py` çalıştırılır.
4. KiCad CLI `pcb drc --refill-zones --save-board --schematic-parity` ve `sch erc` çalıştırılır.
5. `verify_design.py` çalıştırılır; dışa aktarımlar son PCB'den yenilenir.

## Referanslar

- [Espressif DevKitC V4 ölçü çizimi](https://dl.espressif.com/dl/schematics/esp32_devkitc_v4_dimensions.pdf)
- [Espressif DevKitC V4 şeması](https://dl.espressif.com/dl/schematics/esp32_devkitc_v4-sch.pdf)
- [Espressif DevKitC pinleri ve besleme seçenekleri](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/hw-reference/esp32/get-started-devkitc.html)
- [NXP PCF8574 veri sayfası](https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf)
- [onsemi 2N7000 veri sayfası](https://www.onsemi.com/pdf/datasheet/nds7002a-d.pdf)
- [onsemi BC337 veri sayfası](https://www.onsemi.com/pdf/datasheet/bc337-fsc-d.pdf)
- SD modülü: kullanıcının paylaştığı pin etiketleri ve 42 × 24 mm boyut görseli.
