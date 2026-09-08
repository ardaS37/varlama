"""Reproducible KiCad carrier design. Run with KiCad's bundled Python."""
from pathlib import Path
import json, uuid, math, csv
import pcbnew as p

ROOT = Path(__file__).resolve().parent
NAME = 'yoklama-dip-v1'
LIB = ROOT / 'Yoklama.pretty'
LIB.mkdir(exist_ok=True)
UID = lambda s: str(uuid.uuid5(uuid.NAMESPACE_URL, 'yoklama/dip-v1/' + s))
SCH_ID = UID('schematic')
mm = p.FromMM
pt = lambda x,y: p.VECTOR2I(mm(x), mm(y))
board = p.BOARD()
board.SetCopperLayerCount(2)
board.GetDesignSettings().m_MinClearance = mm(.25)
board.GetDesignSettings().m_CopperEdgeClearance = mm(.5)
board.GetDesignSettings().m_TrackMinWidth = mm(.3)
board.GetDesignSettings().m_HoleClearance = mm(.25)
nets, components = {}, []

def net(name):
    if not name: return None
    if name not in nets:
        n=p.NETINFO_ITEM(board,'/'+name,len(nets)+1); board.Add(n); nets[name]=n
    return nets[name]

def line(owner,a,b,layer=p.F_SilkS,width=.15):
    s=p.PCB_SHAPE(); s.SetShape(p.SHAPE_T_SEGMENT); s.SetStart(pt(*a)); s.SetEnd(pt(*b)); s.SetLayer(layer); s.SetWidth(mm(width)); owner.Add(s)

def rect(owner,x1,y1,x2,y2,layer=p.F_SilkS,width=.15):
    for a,b in [((x1,y1),(x2,y1)),((x2,y1),(x2,y2)),((x2,y2),(x1,y2)),((x1,y2),(x1,y1))]:line(owner,a,b,layer,width)

def text(label,x,y,size=1,layer=p.F_SilkS):
    t=p.PCB_TEXT(board); t.SetText(label);t.SetPosition(pt(x,y));t.SetTextSize(pt(size,size));t.SetTextThickness(mm(.15));t.SetLayer(layer);board.Add(t)

def component(ref,value,pins,xy,shape='header',sch=(40,40),description=''):
    # pins: (number, name, net, local x, local y, electrical type)
    f=p.FOOTPRINT(board); f.SetReference(ref); f.SetValue(value); f.SetAttributes(p.FP_THROUGH_HOLE)
    fid='Yoklama:'+ref+'_'+shape
    f.SetFPID(p.LIB_ID('Yoklama',ref+'_'+shape))
    f.SetPath(p.KIID_PATH('/'+SCH_ID+'/'+UID(ref)))
    for num,label,n,x,y,typ in pins:
        pad=p.PAD(f); pad.SetNumber(str(num));pad.SetAttribute(p.PAD_ATTRIB_PTH)
        pad.SetShape(p.PAD_SHAPE_RECT if str(num)=='1' else p.PAD_SHAPE_CIRCLE)
        pad.SetPosition(pt(x,y));pad.SetSize(pt(1.8,1.8));pad.SetDrillSize(pt(1,1));pad.SetLayerSet(pad.PTHMask())
        if n:pad.SetNet(net(n))
        f.Add(pad)
    xs=[a[3] for a in pins];ys=[a[4] for a in pins]
    # Fab outlines indicate component body, not an invented module mounting pattern.
    bounds=(min(xs)-1.5,min(ys)-1.5,max(xs)+1.5,max(ys)+1.5)
    if shape=='esp':bounds=(-1.27,-6.04,26.67,49.53)
    elif shape=='sd':bounds=(-3,-5.65,39,18.35)
    elif shape=='rfid':bounds=(-11.11,-54,28.89,6)
    elif shape=='r':bounds=(1.7,-1.25,5.92,1.25)
    elif shape=='to92':bounds=(-.1,-2.6,5.18,2)
    elif shape=='led':bounds=(-1.23,-2.5,3.77,2.5)
    elif shape=='cap':bounds=(-1.5,-2.5,4.04,2.5)
    elif shape=='buzzer':bounds=(-2.2,-6,9.82,6)
    rect(f,*bounds,p.F_Fab)
    if shape=='r':
        rect(f,*bounds,p.F_SilkS)
        if ref=='D5':line(f,(2.2,-1.25),(2.2,1.25))
    elif shape in ('led','cap','buzzer'):
        s=p.PCB_SHAPE();s.SetShape(p.SHAPE_T_CIRCLE);s.SetCenter(pt(1.27 if shape!='buzzer' else 3.81,0));s.SetEnd(pt((1.27 if shape!='buzzer' else 3.81)+(2.5 if shape!='buzzer' else 6),0));s.SetLayer(p.F_SilkS);s.SetWidth(mm(.15));f.Add(s)
        if shape=='led':line(f,(-1.0,-1.0),(-1.0,1.0))
    elif shape=='to92':
        line(f,(0,-2.3),(5.08,-2.3));line(f,(0,2.2),(5.08,2.2))
    elif shape=='header':
        rect(f,*bounds,p.F_SilkS)
    else:
        rect(f,*bounds,p.F_SilkS)
    # Deliberate component envelope, clear of the pads.
    bx1,by1,bx2,by2=bounds
    rect(f,min(bx1,min(xs)-1.2)-.25,min(by1,min(ys)-1.2)-.25,max(bx2,max(xs)+1.2)+.25,max(by2,max(ys)+1.2)+.25,p.F_CrtYd,.05)
    f.Reference().SetPosition(pt((min(xs)+max(xs))/2,min(by1,min(ys)-1)-1.5))
    f.Reference().SetTextSize(pt(1,1));f.Reference().SetTextThickness(mm(.15))
    if ref=='J3':f.Reference().SetPosition(pt(-4,0))
    f.Value().SetVisible(False)
    # Save the exact library footprint before its board translation.
    p.PCB_IO_MGR.FindPlugin(p.PCB_IO_MGR.KICAD_SEXP).FootprintSave(str(LIB),f)
    f.SetPosition(pt(*xy));board.Add(f)
    sch=tuple(round(round(v/1.27)*1.27,4) for v in sch)
    c=dict(ref=ref,value=value,pins=pins,xy=xy,shape=shape,sch=sch,description=description,fp=fid,uuid=UID(ref))
    components.append(c);return c

def header(ref,value,labels,xy,sch,horizontal=False,shape='header',types=None):
    return component(ref,value,[(i+1,label,n,2.54*i if horizontal else 0,0 if horizontal else 2.54*i,(types or {}).get(label,'passive')) for i,(label,n) in enumerate(labels)],xy,shape,sch)

left=['3V3','EN','VP','VN','IO34','IO35','IO32','IO33','IO25','IO26','IO27','IO14','IO12','GND','IO13','SD2','SD3','CMD','5V']
right=['GND','IO23','IO22','TX0','RX0','IO21','GND','IO19','IO18','IO5','IO17','IO16','IO4','IO0','IO2','IO15','SD1','SD0','CLK']
mapping={'3V3':'+3V3','5V':'+5V_USB','GND':'GND','IO32':'BUZZER_GPIO32','IO33':'SD_CS_GPIO33','IO25':'SD_MISO_GPIO25','IO26':'SD_MOSI_GPIO26','IO27':'RFID_RST_GPIO27','IO14':'SD_SCK_GPIO14','IO13':'LED_NET_GPIO13','IO23':'RFID_MOSI_GPIO23','IO22':'SCL_3V3_GPIO22','IO21':'SDA_3V3_GPIO21','IO19':'RFID_MISO_GPIO19','IO18':'RFID_SCK_GPIO18','IO5':'RFID_CS_GPIO5','IO17':'LED_ERROR_GPIO17','IO16':'LED_OK_GPIO16','IO4':'LED_READY_GPIO4'}
pins=[]
for side,labels in enumerate([left,right]):
    for i,label in enumerate(labels):
        typ='bidirectional'
        if label in ('3V3','5V'):typ='power_out'
        elif label=='GND':typ='passive'
        elif label in ('IO25','IO19'):typ='input'
        elif label in ('IO34','IO35','VP','VN','EN'):typ='input'
        elif label not in ('IO21','IO22') and label in mapping:typ='output'
        pins.append((side*19+i+1,label,mapping.get(label),side*25.4,i*2.54,typ))
component('U1','ESP32-DevKitC 38 pin',pins,(12.7,12.7),'esp',(46,66),'WROOM, 2x19, 25.40 mm row spacing; USB supply only')
header('J1','RC522 module', [('SDA/SS','RFID_CS_GPIO5'),('SCK','RFID_SCK_GPIO18'),('MOSI','RFID_MOSI_GPIO23'),('MISO','RFID_MISO_GPIO19'),('IRQ',None),('GND','GND'),('RST','RFID_RST_GPIO27'),('3V3','+3V3')],(102.87,59.69),(140,43),True,'rfid',{'SDA/SS':'input','SCK':'input','MOSI':'input','MISO':'output','IRQ':'output','RST':'input','3V3':'power_in','GND':'power_in'})
header('J2','MicroSD 42x24 module',[('GND','GND'),('VCC','+5V_USB'),('MISO','SD_MISO_GPIO25'),('MOSI','SD_MOSI_GPIO26'),('SCK','SD_SCK_GPIO14'),('CS','SD_CS_GPIO33')],(107.95,85.09),(230,43),False,'sd',{'VCC':'power_in','GND':'power_in','MISO':'output','MOSI':'input','SCK':'input','CS':'input'})
header('J3','LCD1602 I2C 5V',[('GND','GND'),('VCC','+5V_USB'),('SDA','SDA_5V'),('SCL','SCL_5V')],(55.88,154.94),(330,43),True,types={'GND':'power_in','VCC':'power_in','SDA':'bidirectional','SCL':'input'})

def resistor(ref,value,n1,n2,xy,sch):
    component(ref,value,[(1,'1',n1,0,0,'passive'),(2,'2',n2,7.62,0,'passive')],xy,'r',sch,'Axial 1/4 W, lead pitch 7.62 mm')
def cap(ref,value,n1,n2,xy,sch):
    component(ref,value,[(1,'+',n1,0,0,'passive'),(2,'-',n2,2.54,0,'passive')],xy,'cap',sch,'Radial pitch 2.54 mm')
for idx,(name,x,gpio,color) in enumerate([('READY',50.8,4,'blue'),('OK',63.5,16,'green'),('ERROR',76.2,17,'red'),('NET',88.9,13,'yellow')],1):
    resistor('R'+str(idx),'1k',f'LED_{name}_GPIO{gpio}',f'LED_{name}_A',(x,165.1),(40+(idx-1)*90,155))
    component('D'+str(idx),f'LED {color}',[(1,'K','GND',0,0,'passive'),(2,'A',f'LED_{name}_A',2.54,0,'passive')],(x+2.54,175.26),'led',(40+(idx-1)*90,177),'5 mm LED, 2.54 mm lead pitch')
    text(name,x+3.81,180.34,1)

# I2C MOSFET level shifter, TO-92 2N7000 source-gate-drain (1,2,3).
for idx,sig,x,schx in [(1,'SDA',50.8,310),(2,'SCL',73.66,365)]:
    low=f'{sig}_3V3_GPIO'+('21' if sig=='SDA' else '22')
    component('Q'+str(idx),'2N7000',[(1,'S',low,0,0,'passive'),(2,'G','+3V3',2.54,0,'input'),(3,'D',sig+'_5V',5.08,0,'passive')],(x,104.14),'to92',(schx,92),'onsemi 2N7000, TO-92, spread leads to 2.54 mm pitch')
    resistor('R'+str(3+idx*2),'4.7k','+3V3',low,(x,97.79),(schx,69))
    resistor('R'+str(4+idx*2),'4.7k','+5V_USB',sig+'_5V',(x,111.76),(schx,116))

# Passive piezo sounder: GPIO -> 1k -> NPN, 100k base pull-down.
resistor('R9','1k','BUZZER_GPIO32','BUZZER_BASE',(12.7,81.28),(45,215))
resistor('R10','100k','BUZZER_BASE','GND',(12.7,91.44),(105,215))
component('Q3','BC337-40',[(1,'C','BUZZER_LOW',0,0,'passive'),(2,'B','BUZZER_BASE',2.54,0,'input'),(3,'E','GND',5.08,0,'passive')],(12.7,104.14),'to92',(165,215),'TO-92 BC337: 1 collector, 2 base, 3 emitter')
component('BZ1','Passive piezo 5V',[(1,'+','+5V_USB',0,0,'passive'),(2,'-','BUZZER_LOW',7.62,0,'passive')],(26.67,116.84),'buzzer',(230,215),'Passive piezo, 7.62 mm pitch, <=12 mm body, <=20mA; not an active buzzer')
resistor('R11','10k','+5V_USB','BUZZER_LOW',(25.4,101.6),(290,215))
component('D5','1N4148',[(1,'K','+5V_USB',0,0,'passive'),(2,'A','BUZZER_LOW',7.62,0,'passive')],(25.4,106.68),'r',(355,215),'Axial diode, cathode band at pad 1')
cap('C1','100uF 10V','+5V_USB','GND',(97.79,81.28),(48,243.84))
cap('C2','100nF','+5V_USB','GND',(96.52,102.87),(118,243.84))
cap('C3','100nF','+3V3','GND',(123.19,71.12),(188,243.84))
cap('C4','10uF 10V','+3V3','GND',(132.08,71.12),(258,243.84))
cap('C5','100nF','+5V_USB','GND',(88.9,116.84),(328,243.84))
# Pull-ups keep both readers deselected during boot (GPIO5 is also a strap).
resistor('R12','10k','+3V3','RFID_CS_GPIO5',(81.28,58.42),(140,93))
resistor('R13','10k','+3V3','SD_CS_GPIO33',(87.63,69.85),(230,93))

rect(board,0,0,152.4,187.96,p.Edge_Cuts,.05)
for i,(x,y) in enumerate([(5.08,5.08),(147.32,5.08),(5.08,182.88),(147.32,182.88)],1):
    f=p.FOOTPRINT(board);f.SetReference('H'+str(i));f.SetValue('M3');f.SetAttributes(p.FP_EXCLUDE_FROM_BOM|p.FP_EXCLUDE_FROM_POS_FILES|p.FP_BOARD_ONLY)
    pad=p.PAD(f);pad.SetAttribute(p.PAD_ATTRIB_NPTH);pad.SetShape(p.PAD_SHAPE_CIRCLE);pad.SetSize(pt(3.2,3.2));pad.SetDrillSize(pt(3.2,3.2));pad.SetLayerSet(pad.UnplatedHoleMask());f.Add(pad);f.Reference().SetVisible(False);f.Value().SetVisible(False);f.SetPosition(pt(x,y));board.Add(f)
text('YOKLAMA / DIP v1',53.34,5.08,1.8)
text('USB 5V ONLY',24.13,67.31,1)
text('ESP32-WROOM / 38P',25.4,73.66,1)
text('RC522 / 3V3',113.03,68.58,1)
text('MICRO SD / 5V',127,107.95,1)
text('LCD I2C / 5V',59.69,159.0,1)
rect(board,44.45,124.46,124.45,160.46,p.F_SilkS)
text('LCD 1602 / HORIZONTAL',84.45,135,1.5)
text('80 x 36 mm BODY RESERVATION',84.45,140,1)
text('HEADER POSITION TO VERIFY',84.45,145,1)
for i,s in enumerate(['G','5V','DA','CL']): text(s,55.88+2.54*i,152.4,.8)
for i,s in enumerate(['GND','5V','MISO','MOSI','SCK','CS']):text(s,102.5,85.09+i*2.54,.8)
text('2N7000: S G D',65,108.0,.8)
text('RC522 ANTENNA / NO COPPER',112,18,1,p.Dwgs_User)
text('ESP ANTENNA / NO COPPER',25.4,2.54,.8,p.Dwgs_User)

# RF keepout zones exclude copper on both layers, including vias and pours.
keepouts=[(14,0,36.8,13.0),(90,0,135,51)]
for box in keepouts:
    z=p.ZONE(board);z.SetIsRuleArea(True);z.SetLayerSet(p.LSET.AllCuMask(2));z.SetDoNotAllowTracks(True);z.SetDoNotAllowVias(True);z.SetDoNotAllowZoneFills(True);z.SetDoNotAllowPads(True)
    poly=z.Outline();poly.NewOutline()
    x1,y1,x2,y2=box
    for x,y in [(x1,y1),(x2,y1),(x2,y2),(x1,y2)]:poly.Append(mm(x),mm(y))
    board.Add(z)
p.SaveBoard(str(ROOT/(NAME+'.kicad_pcb')),board)

# Local, self-contained symbol library. Module symbols show physical pad numbers
# and functional pin names; support parts are explicitly named terminal blocks.
def q(s):return json.dumps(str(s),ensure_ascii=False)
def eff(size=1):return f'(effects (font (size {size} {size})))'
def symbol_def(c,embedded=False):
    name=c['ref']+'_'+c['shape']; full='Yoklama:'+name if embedded else name
    n=len(c['pins']); h=max(5.08,(n-1)*1.27+3.81)
    out=[f'(symbol {q(full)} (pin_names (offset 0.6)) (in_bom yes) (on_board yes)',
         f'(property "Reference" {q(c["ref"].rstrip("0123456789"))} (at 0 {h+2.54} 0) {eff()})',
         f'(property "Value" {q(c["value"])} (at 0 {h+5.08} 0) {eff()})',
         f'(property "Footprint" {q(c["fp"])} (at 0 0 0) (effects (font (size 1 1)) (hide yes)))',
         f'(symbol {q(name+"_0_1")} (rectangle (start -10.16 {h}) (end 10.16 {-h}) (stroke (width .254) (type default)) (fill (type background))))',
         f'(symbol {q(name+"_1_1")}']
    for i,(num,label,nn,x,y,typ) in enumerate(c['pins']):
        yy=(n-1)*1.27-i*2.54
        out.append(f'(pin {typ} line (at -15.24 {yy:.4f} 0) (length 5.08) (name {q(label)} {eff()}) (number {q(num)} {eff()}))')
    out.append('))');return '\n'.join(out)

lib='(kicad_symbol_lib (version 20250114) (generator "kicad_symbol_editor")\n'+'\n'.join(symbol_def(c) for c in components)+'\n)'
(ROOT/'Yoklama.kicad_sym').write_text(lib,encoding='utf-8')
(ROOT/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "Yoklama") (type "KiCad") (uri "${KIPRJMOD}/Yoklama.kicad_sym") (options "") (descr "Yoklama carrier symbols")))',encoding='utf-8')
(ROOT/'fp-lib-table').write_text('(fp_lib_table (version 7) (lib (name "Yoklama") (type "KiCad") (uri "${KIPRJMOD}/Yoklama.pretty") (options "") (descr "Direct solder carrier footprints")))',encoding='utf-8')
out=[f'(kicad_sch (version 20250114) (generator "eeschema") (uuid {SCH_ID}) (paper "A3")',
     '(title_block (title "Yoklama DIP carrier / direct solder modules") (date "2026-09-08") (rev "1.0-prototype") (comment 1 "USB power only; ESP32 DevKitC 38 pins; 2.54 mm pitch / 25.4 mm rows"))',
     '(lib_symbols '+'\n'.join(symbol_def(c,True) for c in components)+')']
for c in components:
    x,y=c['sch'];n=len(c['pins']);h=max(5.08,(n-1)*1.27+3.81)
    out.append(f'(symbol (lib_id {q("Yoklama:"+c["ref"]+"_"+c["shape"])}) (at {x} {y} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid {c["uuid"]})')
    for key,val,py,hide in [('Reference',c['ref'],y-h-3.81,False),('Value',c['value'],y-h-1.27,False),('Footprint',c['fp'],y,True)]:
        out.append(f'(property {q(key)} {q(val)} (at {x} {py:.4f} 0) (effects (font (size 1 1))'+(' (hide yes)' if hide else '')+'))')
    for num,*_ in c['pins']:out.append(f'(pin {q(num)} (uuid {UID(c["ref"]+"pin"+str(num))}))')
    out.append(f'(instances (project {q(NAME)} (path {q("/"+SCH_ID)} (reference {q(c["ref"])}) (unit 1)))))')
    for i,(num,label,nn,px,py,typ) in enumerate(c['pins']):
        xx=x-15.24; yy=y-((n-1)*1.27-i*2.54)
        if nn:
            end=xx-7.62
            out.append(f'(wire (pts (xy {xx:.4f} {yy:.4f}) (xy {end:.4f} {yy:.4f})) (stroke (width 0) (type default)) (uuid {UID(c["ref"]+"wire"+str(num))}))')
            out.append(f'(label {q(nn)} (at {end:.4f} {yy:.4f} 0) (effects (font (size .85 .85)) (justify left bottom)) (uuid {UID(c["ref"]+"label"+str(num))}))')
        else:out.append(f'(no_connect (at {xx:.4f} {yy:.4f}) (uuid {UID(c["ref"]+"nc"+str(num))}))')
# Ground is supplied by the module's USB connector. A power flag documents it.
flag={'ref':'#FLG01','value':'PWR_FLAG','pins':[(1,'pwr','GND',0,0,'power_out')],'shape':'power','fp':'','sch':(0,0)}
# Use standard flag-shaped embedded symbol, with no board footprint.
flag_def='(symbol "Yoklama:GND_FLAG" (pin_names (offset 0)) (in_bom no) (on_board no) (property "Reference" "#FLG" (at 0 0 0) (effects (font (size 1 1)) (hide yes))) (property "Value" "PWR_FLAG" (at 0 2.54 0) (effects (font (size 1 1)))) (symbol "GND_FLAG_0_1" (polyline (pts (xy 0 0) (xy 0 1.27) (xy -1.27 1.27) (xy 0 2.54) (xy 1.27 1.27) (xy 0 1.27)) (stroke (width .15) (type default)) (fill (type none)))) (symbol "GND_FLAG_1_1" (pin power_out line (at 0 0 90) (length 0) (name "pwr" (effects (font (size 1 1)))) (number "1" (effects (font (size 1 1)))))))'
out[2]=out[2][:-1]+'\n'+flag_def+')'
(ROOT/'Yoklama.kicad_sym').write_text(lib[:-1]+flag_def.replace('"Yoklama:GND_FLAG"','"GND_FLAG"')+')',encoding='utf-8')
out.append(f'(symbol (lib_id "Yoklama:GND_FLAG") (at 390 260 0) (unit 1) (in_bom no) (on_board no) (dnp no) (uuid {UID("flag")}) (property "Reference" "#FLG01" (at 390 260 0) (effects (font (size 1 1)) (hide yes))) (property "Value" "PWR_FLAG" (at 390 256.19 0) {eff()}) (pin "1" (uuid {UID("flagpin")})) (instances (project {q(NAME)} (path {q("/"+SCH_ID)} (reference "#FLG01") (unit 1)))))')
out.append(f'(label "GND" (at 390 260 0) (effects (font (size 1 1)) (justify left bottom)) (uuid {UID("flaglabel")}))')
out.append(f'(text "MODULES: match printed pin names. NC = intentionally unused.\\nUSB is the only power input. LCD: 16x2 / PCF8574 / address 0x3F.\\nAll carrier parts are through-hole. Read README before fabrication." (at 105 130 0) (effects (font (size 1.1 1.1)) (justify left)) (uuid {UID("notes")}))')
out.append('(sheet_instances (path "/" (page "1")))\n)')
(ROOT/(NAME+'.kicad_sch')).write_text('\n'.join(out).replace('390 260','389.89 245.11').replace('390 256.19','389.89 241.3'),encoding='utf-8')
project={'meta':{'filename':NAME+'.kicad_pro','version':1},'board':{'design_settings':{'rules':{'min_clearance':.25,'min_track_width':.3,'min_via_diameter':.8,'min_through_hole_diameter':.3,'min_hole_clearance':.25,'min_copper_edge_clearance':.5},'rule_severities':{'lib_footprint_mismatch':'warning'}}},'net_settings':{'classes':[{'name':'Default','clearance':.25,'track_width':.3,'via_diameter':.8,'via_drill':.4,'microvia_diameter':.3,'microvia_drill':.1,'diff_pair_width':.3,'diff_pair_gap':.25,'diff_pair_via_gap':.25,'pcb_color':'rgba(0, 0, 0, 0.000)','schematic_color':'rgba(0, 0, 0, 0.000)','wire_width':6,'bus_width':12,'line_style':0},{'name':'Power','clearance':.25,'track_width':.8,'via_diameter':1,'via_drill':.5,'microvia_diameter':.3,'microvia_drill':.1,'diff_pair_width':.3,'diff_pair_gap':.25,'diff_pair_via_gap':.25,'pcb_color':'rgba(0, 0, 0, 0.000)','schematic_color':'rgba(0, 0, 0, 0.000)','wire_width':6,'bus_width':12,'line_style':0}],'netclass_patterns':[{'netclass':'Power','pattern':n} for n in ['GND','+3V3','+5V_USB']],'meta':{'version':4}}}
(ROOT/(NAME+'.kicad_pro')).write_text(json.dumps(project,indent=2),encoding='utf-8')
(ROOT/'design.json').write_text(json.dumps({'name':NAME,'components':components,'keepouts':keepouts,'board_mm':[152.4,187.96]},indent=2),encoding='utf-8')
with (ROOT/'BOM.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['Reference','Value','Footprint','Notes'])
    for c in components:w.writerow([c['ref'],c['value'],c['fp'],c['description']])
print(f'Created {len(components)} components, {len(nets)} nets, PCB and schematic')


