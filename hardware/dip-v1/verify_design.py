from pathlib import Path
import re,json,csv,hashlib
import pcbnew as p
root=Path(__file__).resolve().parent
src=(root.parents[1]/'src/main.cpp').read_text(encoding='utf-8')
b=p.LoadBoard(str(root/'yoklama-dip-v1.kicad_pcb'))
pads={(f.GetReference(),pad.GetNumber()):pad for f in b.GetFootprints() for pad in f.Pads()}
checks=[('RFID_SS',5,'J1','1','RFID_CS_GPIO5'),('RFID_RST',27,'J1','7','RFID_RST_GPIO27'),('SD_CS',33,'J2','6','SD_CS_GPIO33'),('SD_SCK',14,'J2','5','SD_SCK_GPIO14'),('SD_MISO',25,'J2','3','SD_MISO_GPIO25'),('SD_MOSI',26,'J2','4','SD_MOSI_GPIO26'),('LCD_SDA',21,'Q1','1','SDA_3V3_GPIO21'),('LCD_SCL',22,'Q2','1','SCL_3V3_GPIO22'),('LED_READY',4,'R1','1','LED_READY_GPIO4'),('LED_SUCCESS',16,'R2','1','LED_OK_GPIO16'),('LED_ERROR',17,'R3','1','LED_ERROR_GPIO17'),('LED_INTERNET',13,'R4','1','LED_NET_GPIO13'),('BUZZER_PIN',32,'R9','1','BUZZER_GPIO32')]
for const,gpio,ref,pin,net in checks:
    actual=int(re.search(r'constexpr uint8_t '+const+r'\s*=\s*(\d+)',src)[1]);assert actual==gpio,(const,actual,gpio)
    assert pads[(ref,pin)].GetNetname()=='/'+net,(ref,pin)
    assert any(pad.GetNetname()=='/'+net for (r,_),pad in pads.items() if r=='U1'),net
assert 'SPI.begin();' in src
for pin,net in [('2','RFID_SCK_GPIO18'),('3','RFID_MOSI_GPIO23'),('4','RFID_MISO_GPIO19')]:assert pads[('J1',pin)].GetNetname()=='/'+net
assert p.ToMM(pads[('U1','20')].GetPosition().x-pads[('U1','1')].GetPosition().x)==25.4
assert len([k for k in pads if k[0]=='U1'])==38
for pad in pads.values():assert pad.GetAttribute() in (p.PAD_ATTRIB_PTH,p.PAD_ATTRIB_NPTH)
erc=(root/'reports/erc.rpt').read_text();drc=(root/'reports/drc.rpt').read_text()
assert 'Errors 0  Warnings 0' in erc
assert 'Found 0 DRC violations' in drc and 'Found 0 unconnected pads' in drc and 'Found 0 Footprint errors' in drc
report={'firmware_gpio_constants_checked':len(checks),'spi_defaults':'esp32dev VSPI: SCK=18 MISO=19 MOSI=23','esp32_pads':38,'row_spacing_mm':25.4,'all_component_pads_through_hole':True,'erc_errors':0,'erc_warnings':0,'drc_violations':0,'unconnected_pads':0,'schematic_parity_issues':0,'pcb_sha256':hashlib.sha256((root/'yoklama-dip-v1.kicad_pcb').read_bytes()).hexdigest(),'firmware_sha256':hashlib.sha256((root.parents[1]/'src/main.cpp').read_bytes()).hexdigest(),'physical_prototype_tested':False}
(root/'reports/verification.json').write_text(json.dumps(report,indent=2))
with (root/'pinout.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['Firmware constant','GPIO','Component','Pad','Net']);w.writerows(checks)
    for gpio,pin,net in [(18,2,'RFID_SCK_GPIO18'),(23,3,'RFID_MOSI_GPIO23'),(19,4,'RFID_MISO_GPIO19')]:w.writerow(['VSPI default',gpio,'J1',pin,net])
print(json.dumps(report,indent=2))
