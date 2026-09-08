from pathlib import Path
import re
import pcbnew as p
ROOT=Path(__file__).resolve().parent
path=ROOT/'yoklama-dip-v1.kicad_pcb'
b=p.LoadBoard(str(path))
existing_pour_layers={z.GetLayer() for z in b.Zones() if not z.GetIsRuleArea()}
labels=[('ESP32',25.4,33.02,2),('38 PIN',25.4,38.1,1.2),('RC522',112,30,3),('MICRO SD',129,92,1.8)]
existing=path.read_text(encoding='utf-8')
for label,x,y,size in labels:
    if re.search(r'\(gr_text\s+"'+re.escape(label)+'"',existing):continue
    t=p.PCB_TEXT(b);t.SetText(label);t.SetPosition(p.VECTOR2I(p.FromMM(x),p.FromMM(y)));t.SetTextSize(p.VECTOR2I(p.FromMM(size),p.FromMM(size)));t.SetTextThickness(p.FromMM(.2));t.SetLayer(p.F_SilkS);b.Add(t)
gnd=next(n for n in b.GetNetsByNetcode().values() if n.GetNetname()=='/GND')
for layer in (p.F_Cu,p.B_Cu):
    if layer in existing_pour_layers:continue
    z=p.ZONE(b);z.SetLayer(layer);z.SetNet(gnd);z.SetLocalClearance(p.FromMM(.3));z.SetThermalReliefGap(p.FromMM(.3));z.SetThermalReliefSpokeWidth(p.FromMM(.4));z.SetPadConnection(p.ZONE_CONNECTION_THERMAL);z.SetMinThickness(p.FromMM(.25))
    poly=z.Outline();poly.NewOutline()
    for x,y in [(1,1),(151.4,1),(151.4,186.96),(1,186.96)]:poly.Append(p.FromMM(x),p.FromMM(y))
    b.Add(z)
p.SaveBoard(str(path),b)
print('Added GND pours on both copper layers; use CLI refill + DRC.')
