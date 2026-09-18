from pathlib import Path
import json, math, xml.etree.ElementTree as ET, zipfile
from PIL import Image, ImageDraw, ImageFont

OUT=Path(__file__).parent
parts=[]
def add(ref,value,x,y,pins,coords=None,pitch=2.54,notes=''):
    coords=coords or [(0,i*pitch) for i in range(len(pins))]
    parts.append(dict(ref=ref,value=value,x=x,y=y,pins=pins,coords=coords,notes=notes))
def dual(ref,value,x,y,left,right,spacing,notes=''):
    add(ref,value,x,y,left+right,[(0,i*2.54) for i in range(len(left))]+[(spacing,i*2.54) for i in range(len(right))],notes=notes)
dual('U1','ESP32_DEVKIT_V1_30PIN_ASSUMED',24,17,
 ['+5V','GND','NC_D15','NC_D2','CAL','NC_RX2','NC_TX2','NC_D5','BIN1','BIN2','GND','SDA','NC_RX0','NC_TX0','STBY'],
 ['+3V3','GND','PWMB','NC_D12','DISP_DIN','AIN2','AIN1','PWMA','ENC_L_B','ENC_L_A','ENC_R_A','ENC_R_B','NC_VN','NC_VP','NC_EN'],25.4,
 'USB faces rear; antenna faces front. Row spacing 25.4 mm ASSUMED. IMPORTANT: photographed board order replaces the preliminary order below.')
# Correct 30-pin DEVKIT photo order, USB end first. Left VIN,GND,D13,D12,D14,D27,D26,D25,D33,D32,D35,D34,VN,VP,EN.
parts[-1]['pins']=['+5V','GND','DISP_DIN','NC_D12','PWMB','AIN2','AIN1','PWMA','DISP_CS','DISP_CLK','ENC_L_B','ENC_L_A','ENC_R_B','ENC_R_A','NC_EN']+['+3V3','GND','NC_D15','NC_D2','CAL','NC_RX2','NC_TX2','NC_D5','BIN1','BIN2','SDA','NC_RX0','NC_TX0','SCL','STBY']
parts[-1]['notes']='Component-side top view, USB at rear. Left row VIN..EN; right row 3V3..D23. 25.4 mm row spacing and 2.54 mm pitch ASSUMED; compare physical board before fabrication.'
dual('U2','TB6612FNG_MODULE_ASSUMED',90,40,
 ['PWMA','AIN2','AIN1','STBY','BIN1','BIN2','PWMB','GND'],
 ['VM','+3V3','GND','M_L_P','M_L_N','M_R_N','M_R_P','GND'],15.24,
 'Pin sequence from supplied labelled face; verify which face is upward when plugged in. 15.24 mm row spacing ASSUMED.')
add('U3','MPU9250_6500_MODULE_ASSUMED',82,9,
 ['+3V3','GND','SCL','SDA','NC_EDA','NC_ECL','GND','NC_INT','+3V3','GND'],notes='2.54 mm pitch ASSUMED; AD0 low, NCS high, FSYNC low. Module VCC regulator/dropout must be checked; sensor identity unconfirmed.')
add('J1','LEFT_MOTOR_JST_PH_6_ASSUMED',7,45,['M_L_N','M_L_P','+3V3','GND','ENC_L_A','ENC_L_B'],pitch=2,notes='2.0 mm pitch PLACEHOLDER, not validated JST housing. Cable pin order is proposed, not measured.')
add('J2','RIGHT_MOTOR_JST_PH_6_ASSUMED',123,45,['M_R_N','M_R_P','+3V3','GND','ENC_R_A','ENC_R_B'],pitch=2,notes='Same assumptions as J1; encoder supply and logic must support 3.3 V.')
for ref,x,data in [('J3',36,'DIN_5V'),('J4',78,'CHAIN_DOUT')]:
    add(ref,'EYE_INPUT_5PIN',x,71,['+5V','GND',data,'CS_5V','CLK_5V'],[(i*2.54,0) for i in range(5)])
add('J5','EYE1_DOUT_RETURN',63,70,['CHAIN_DOUT','GND'],[(0,0),(2.54,0)],notes='Connect eye 1 output DOUT here. J4 DIN gets this net, never ESP32 input.')
add('J6','REAR_CAL_BUTTON',64,5,['GND','CAL','+5V','GND'],[(i*2.54,0) for i in range(4)],notes='COM,NO,LED+,LED-. NC unused. Actual switch terminals must be identified.')
add('J7','REGULATED_5V_INPUT',108,7,['+5V','GND'],[(0,0),(5.08,0)],notes='External regulated 5 V only; no raw battery. Fuse and reverse-polarity protection upstream.')
add('J8','REGULATED_MOTOR_INPUT',117,25,['VM','GND'],[(0,0),(5.08,0)],notes='External regulated nominal 3 V for stated 3 V motors. Supply/driver current suitability pending stall current.')
dual('U4','SN74AHCT125N_DIP14',62,44,['GND','DISP_DIN','DIN_5V','GND','DISP_CS','CS_5V','GND'],['+5V','+5V','GND','NC_BUF_OUT','GND','DISP_CLK','CLK_5V'],7.62,
 'PCB pad numbering is explicit DIP numbering. 5 V supply, three channels active, fourth disabled.')
parts[-1]['numbers']=list(range(1,8))+list(range(14,7,-1))
for i,(net,x,y) in enumerate([('ENC_L_A',15,38),('ENC_L_B',15,42),('ENC_R_A',61,24),('ENC_R_B',61,28),('CAL',63,13)],1):
    add('R'+str(i),'10k',x,y,[net,'+3V3'],[(0,0),(5.08,0)],notes='Encoder pull-ups populate only for verified compatible encoder outputs.')
add('R6','10k_STBY_PULLDOWN',87,63,['STBY','GND'],[(0,0),(5.08,0)])
for i,(net,x,y) in enumerate([('+5V',74,44),('+3V3',88,9),('+3V3',108,38),('VM',110,42)],1):
    add('C'+str(i),'100nF',x,y,[net,'GND'],[(0,0),(2.54,0)])
add('C5','470uF_10V',110,55,['VM','GND'],[(0,0),(5,0)],notes='Polarized: pad 1 positive; 8 mm diameter placeholder.')
add('C6','220uF_10V',101,14,['+5V','GND'],[(0,0),(5,0)],notes='Polarized: pad 1 positive; 8 mm diameter placeholder.')
for i,(net,x) in enumerate([('SDA',64),('SCL',72)],7):
    add('R'+str(i),'4k7_DNP',x,35,[net,'+3V3'],[(0,0),(5.08,0)],notes='Optional I2C pull-up; normally DNP if module already provides pull-ups to 3.3 V.')

nets=sorted({n for p in parts for n in p['pins'] if not n.startswith('NC_')})
ids={n:i+1 for i,n in enumerate(nets)}
pcb=['(kicad_pcb (version 20221018) (generator pcbnew) (general (thickness 1.6)) (paper "A4")',
'(layers (0 "F.Cu" signal) (31 "B.Cu" signal) (36 "B.SilkS" user "b.silkscreen") (37 "F.SilkS" user "f.silkscreen") (38 "B.Mask" user) (39 "F.Mask" user) (44 "Edge.Cuts" user) (46 "B.CrtYd" user) (47 "F.CrtYd" user) (48 "B.Fab" user) (49 "F.Fab" user))',
'(setup (pad_to_mask_clearance 0)) (net 0 "")']
pcb += [f'(net {i} "{n}")' for n,i in ids.items()]
for a,b in [((0,0),(130,0)),((130,0),(130,77)),((130,77),(0,77)),((0,77),(0,0))]:
    pcb.append(f'(gr_line (start {a[0]} {a[1]}) (end {b[0]} {b[1]}) (stroke (width 0.05) (type default)) (layer "Edge.Cuts"))')
for p in parts:
    pcb.append(f'(footprint "ZENO:{p["value"]}" (layer "F.Cu") (at {p["x"]} {p["y"]}) (attr through_hole)')
    pcb.append(f'(fp_text reference "{p["ref"]}" (at 0 -2) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))')
    pcb.append(f'(fp_text value "{p["value"]}" (at 0 -3.5) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))')
    for k,((x,y),n) in enumerate(zip(p['coords'],p['pins'])):
        number=p.get('numbers',list(range(1,len(p['pins'])+1)))[k]
        net='' if n.startswith('NC_') else f'(net {ids[n]} "{n}")'
        pcb.append(f'(pad "{number}" thru_hole {"rect" if number==1 else "circle"} (at {x} {y}) (size 1.7 1.7) (drill 1) (layers "*.Cu" "*.Mask") {net})')
    pcb.append(')')
for i,(x,y) in enumerate([(5,5),(125,5),(5,72),(125,72)],1):
    pcb.append(f'(footprint "ZENO:M3_NPTH" (layer "F.Cu") (at {x} {y}) (attr exclude_from_pos_files exclude_from_bom) (fp_text reference "H{i}" (at 0 -3) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15)))) (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask")))')
for s,x,y in [('ZENO REV A - UNROUTED DRAFT',66,2),('FRONT / EYES',65,75),('VERIFY ALL FOOTPRINT DIMENSIONS',65,66)]:
    pcb.append(f'(gr_text "{s}" (at {x} {y}) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))')
pcb.append(')')
(OUT/'ZENO_RevA_DRAFT.kicad_pcb').write_text('\n'.join(pcb))
(OUT/'ZENO_RevA_DRAFT.kicad_pro').write_text(json.dumps({'meta':{'filename':'ZENO_RevA_DRAFT.kicad_pro','version':1}},indent=2))
root=ET.Element('export',version='E'); comps=ET.SubElement(root,'components')
for p in parts:
    c=ET.SubElement(comps,'comp',ref=p['ref']); ET.SubElement(c,'value').text=p['value']
ns=ET.SubElement(root,'nets')
for net,code in ids.items():
    n=ET.SubElement(ns,'net',code=str(code),name=net)
    for p in parts:
        for i,pn in enumerate(p['pins']):
            if pn==net: ET.SubElement(n,'node',ref=p['ref'],pin=str(p.get('numbers',list(range(1,len(p['pins'])+1)))[i]))
ET.indent(root); ET.ElementTree(root).write(OUT/'ZENO_connectivity.net',encoding='utf-8',xml_declaration=True)
doc=['# ZENO Rev A — placement and connectivity draft','', '**UNROUTED. NOT FOR FABRICATION.** No Gerbers or fabrication release are included. Native KiCad loading and DRC were not available in this environment. Open the board directly in KiCad PCB Editor. Nets provide ratsnest connections; there is no graphical .kicad_sch schematic. The XML netlist is supplementary.','',
'## Mechanical plan','130 × 77 × 1.6 mm, two copper layers. Front is y=77; rear is y=0. Proposed 3.2 mm mounting holes: (5,5), (125,5), (5,72), (125,72) mm. Hole spacing 120 × 67 mm is provisional, not matched to chassis. Module sockets use 1 mm drills and 1.7 mm pads provisionally. No connector body courtyards or final housing geometry have been validated. The preview is a placement drawing, not a rendering of a finished routed PCB.','',
'## Electrical decisions','External regulated +5 V enters J7; external regulated nominal 3 V motor power enters J8. Raw 2S battery must not enter either connector. Use upstream protection and regulator current capacity appropriate to measured loads. Do not simultaneously connect external 5 V and USB until the actual DevKit power-path isolation is checked. VM and +5 V share ground but are not connected together. TB6612 logic VCC is +3V3. Check driver suitability against motor stall current and thermal conditions; no stall-current validation has been performed.','',
'Encoder supply is provisionally 3.3 V. If encoder requires 5 V, redesign supply and level conversion before connection. GPIO34/35/36/39 are input-only; external pull-up footprints are provided. Do not infer cable pin order from colours. Motor minus is a bridge output, not ground.','',
'Displays use three 5 V SN74AHCT125 buffer channels. Eye 1 DOUT must return through J5 to eye 2 DIN at J4. Both displays share CLK and CS. This additional return connection is required for independent eyes in a daisy chain. Alternatively wire module-to-module and revise the harness accordingly.','',
'Calibration: J6 pins 1 COM/GND, 2 NO/GPIO4, 3 LED+/5V, 4 LED-/GND. NC unused. LED continuously on with power if purchased LED supports 5 V. Firmware must debounce and allow calibration only when stationary with motor outputs disabled. This button is not a battery disconnect or emergency stop.','',
'IMU: AD0 low selects 0x68 for compatible devices; NCS held high for I2C, FSYNC low. EDA/ECL/INT unused. Verify sensor identity, VCC regulator dropout, and bus pull-ups on actual board. Optional carrier I2C pull-ups R7/R8 are DNP by default.','',
'## Before routing and fabrication','1. Measure ESP32 row spacing (assumed 25.4 mm), TB6612 row spacing (assumed 15.24 mm), and IMU pitch (assumed 2.54 mm). Confirm module face/orientation against every net, especially TB6612 photograph.\n2. Identify exact six-pin JST family, pitch, mating PCB header and cable orientation. Current J1/J2 pads are 2.0 mm placeholders, not validated connector footprints.\n3. Measure standoffs and switch chassis clearance; confirm motor/encoder voltage and stall current.\n4. Replace provisional footprints with verified body outlines/courtyards, add actual ESP32 antenna copper/component keepout on both sides and maintain USB access.\n5. Route motor paths short and wide, away from IMU and I2C; size copper for measured current and copper weight. Provide ground plane and local decoupling. This draft has ZERO tracks, vias, zones, or enforced keepouts.\n6. Create/verify graphical schematic and ERC, route every net, then run KiCad DRC and review fabrication outputs. Print footprint page at 1:1 and physically test-fit before ordering.','',
'## Sources','- ESP32: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf\n- TB6612FNG: https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf\n- MAX7219: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX7219-MAX7221.pdf\n- SN74AHCT125: https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf\nModule pin labels were taken from user-supplied photographs; module dimensions were not supplied.','', '## Parts and pad assignments']
for p in parts:
    doc += ['',f'### {p["ref"]} — {p["value"]}',p['notes'],'','| Pad | Net |','|---:|---|']
    for i,n in enumerate(p['pins']): doc.append(f'| {p.get("numbers",list(range(1,len(p["pins"])+1)))[i]} | {n} |')
(OUT/'README.md').write_text('\n'.join(doc))
# Exact placement preview, rendered from the same coordinate model as the PCB.
S=10; margin=75
im=Image.new('RGB',(1450,960),'#101820'); d=ImageDraw.Draw(im)
fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f=ImageFont.truetype(fontpath,16); title=ImageFont.truetype(fontpath,25)
def xy(x,y):return(margin+x*S,110+y*S)
d.text((75,24),'ZENO  |  130 × 77 mm  |  PCB PLACEMENT DRAFT',font=title,fill='white')
d.text((75,62),'UNROUTED — provisional footprints — do not fabricate',font=f,fill='#ffbd69')
d.rectangle([xy(0,0),xy(130,77)],fill='#123d34',outline='#9ee5be',width=3)
for x,y in [(5,5),(125,5),(5,72),(125,72)]:
    a,b=xy(x,y); d.ellipse((a-16,b-16,a+16,b+16),fill='#101820',outline='#bbbbbb',width=2)
for p in parts:
    for i,(dx,dy) in enumerate(p['coords']):
        a,b=xy(p['x']+dx,p['y']+dy)
        d.ellipse((a-7,b-7,a+7,b+7),fill='#d4ad50'); d.ellipse((a-3,b-3,a+3,b+3),fill='#18252b')
    a,b=xy(p['x'],p['y']); d.text((a-8,b-23),p['ref'],font=f,fill='white')
for label,x,y in [('ESP32',30,31),('USB ↑ rear',29,12),('Antenna ↓',28,58),('IMU',87,23),('TB6612',92,48),('AHCT125',60,62),('LEFT MOTOR',2,61),('RIGHT MOTOR',109,61),('EYE 1',35,74),('EYE 2',78,74),('CAL',66,8),('5V IN',108,10),('VM IN',117,29)]:
    d.text(xy(x,y),label,font=f,fill='#9fe4ef')
d.text((75,905),'Front: eyes    |    Rear: calibration button    |    Gold circles: through-hole pads',font=f,fill='white')
im.save(OUT/'ZENO_layout_preview.png')
(OUT/'design_data.json').write_text(json.dumps(parts,indent=2))
# Basic structural validation; does not substitute for KiCad load/ERC/DRC.
for p in parts:
    assert len(p['pins'])==len(p['coords'])
    for dx,dy in p['coords']: assert 0<p['x']+dx<130 and 0<p['y']+dy<77
text=(OUT/'ZENO_RevA_DRAFT.kicad_pcb').read_text(); assert text.count('(')==text.count(')')
ET.parse(OUT/'ZENO_connectivity.net')
(OUT/'VALIDATION.txt').write_text(f'{len(parts)} electrical footprints; 4 mounting holes; {len(nets)} named nets.\nPad coordinates checked inside board outline. XML parsed and PCB parentheses balanced.\nNo KiCad parser, ERC, DRC, electrical operation, or mechanical fit validation performed.\nUnrouted: zero copper tracks. Do not fabricate.\n')
with zipfile.ZipFile(OUT.parent/'ZENO_PCB_RevA_Draft.zip','w',zipfile.ZIP_DEFLATED) as z:
    for path in sorted(OUT.iterdir()):
        if path.is_file():z.write(path,'ZENO_PCB_Draft/'+path.name)
print(OUT.parent/'ZENO_PCB_RevA_Draft.zip')
