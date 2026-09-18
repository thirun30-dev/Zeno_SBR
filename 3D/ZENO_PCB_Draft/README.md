# ZENO Rev A — placement and connectivity draft

**UNROUTED. NOT FOR FABRICATION.** No Gerbers or fabrication release are included. Native KiCad loading and DRC were not available in this environment. Open the board directly in KiCad PCB Editor. Nets provide ratsnest connections; there is no graphical .kicad_sch schematic. The XML netlist is supplementary.

## Mechanical plan
130 × 77 × 1.6 mm, two copper layers. Front is y=77; rear is y=0. Proposed 3.2 mm mounting holes: (5,5), (125,5), (5,72), (125,72) mm. Hole spacing 120 × 67 mm is provisional, not matched to chassis. Module sockets use 1 mm drills and 1.7 mm pads provisionally. No connector body courtyards or final housing geometry have been validated. The preview is a placement drawing, not a rendering of a finished routed PCB.

## Electrical decisions
External regulated +5 V enters J7; external regulated nominal 3 V motor power enters J8. Raw 2S battery must not enter either connector. Use upstream protection and regulator current capacity appropriate to measured loads. Do not simultaneously connect external 5 V and USB until the actual DevKit power-path isolation is checked. VM and +5 V share ground but are not connected together. TB6612 logic VCC is +3V3. Check driver suitability against motor stall current and thermal conditions; no stall-current validation has been performed.

Encoder supply is provisionally 3.3 V. If encoder requires 5 V, redesign supply and level conversion before connection. GPIO34/35/36/39 are input-only; external pull-up footprints are provided. Do not infer cable pin order from colours. Motor minus is a bridge output, not ground.

Displays use three 5 V SN74AHCT125 buffer channels. Eye 1 DOUT must return through J5 to eye 2 DIN at J4. Both displays share CLK and CS. This additional return connection is required for independent eyes in a daisy chain. Alternatively wire module-to-module and revise the harness accordingly.

Calibration: J6 pins 1 COM/GND, 2 NO/GPIO4, 3 LED+/5V, 4 LED-/GND. NC unused. LED continuously on with power if purchased LED supports 5 V. Firmware must debounce and allow calibration only when stationary with motor outputs disabled. This button is not a battery disconnect or emergency stop.

IMU: AD0 low selects 0x68 for compatible devices; NCS held high for I2C, FSYNC low. EDA/ECL/INT unused. Verify sensor identity, VCC regulator dropout, and bus pull-ups on actual board. Optional carrier I2C pull-ups R7/R8 are DNP by default.

## Before routing and fabrication
1. Measure ESP32 row spacing (assumed 25.4 mm), TB6612 row spacing (assumed 15.24 mm), and IMU pitch (assumed 2.54 mm). Confirm module face/orientation against every net, especially TB6612 photograph.
2. Identify exact six-pin JST family, pitch, mating PCB header and cable orientation. Current J1/J2 pads are 2.0 mm placeholders, not validated connector footprints.
3. Measure standoffs and switch chassis clearance; confirm motor/encoder voltage and stall current.
4. Replace provisional footprints with verified body outlines/courtyards, add actual ESP32 antenna copper/component keepout on both sides and maintain USB access.
5. Route motor paths short and wide, away from IMU and I2C; size copper for measured current and copper weight. Provide ground plane and local decoupling. This draft has ZERO tracks, vias, zones, or enforced keepouts.
6. Create/verify graphical schematic and ERC, route every net, then run KiCad DRC and review fabrication outputs. Print footprint page at 1:1 and physically test-fit before ordering.

## Sources
- ESP32: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
- TB6612FNG: https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf
- MAX7219: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX7219-MAX7221.pdf
- SN74AHCT125: https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf
Module pin labels were taken from user-supplied photographs; module dimensions were not supplied.

## Parts and pad assignments

### U1 — ESP32_DEVKIT_V1_30PIN_ASSUMED
Component-side top view, USB at rear. Left row VIN..EN; right row 3V3..D23. 25.4 mm row spacing and 2.54 mm pitch ASSUMED; compare physical board before fabrication.

| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |
| 3 | DISP_DIN |
| 4 | NC_D12 |
| 5 | PWMB |
| 6 | AIN2 |
| 7 | AIN1 |
| 8 | PWMA |
| 9 | DISP_CS |
| 10 | DISP_CLK |
| 11 | ENC_L_B |
| 12 | ENC_L_A |
| 13 | ENC_R_B |
| 14 | ENC_R_A |
| 15 | NC_EN |
| 16 | +3V3 |
| 17 | GND |
| 18 | NC_D15 |
| 19 | NC_D2 |
| 20 | CAL |
| 21 | NC_RX2 |
| 22 | NC_TX2 |
| 23 | NC_D5 |
| 24 | BIN1 |
| 25 | BIN2 |
| 26 | SDA |
| 27 | NC_RX0 |
| 28 | NC_TX0 |
| 29 | SCL |
| 30 | STBY |

### U2 — TB6612FNG_MODULE_ASSUMED
Pin sequence from supplied labelled face; verify which face is upward when plugged in. 15.24 mm row spacing ASSUMED.

| Pad | Net |
|---:|---|
| 1 | PWMA |
| 2 | AIN2 |
| 3 | AIN1 |
| 4 | STBY |
| 5 | BIN1 |
| 6 | BIN2 |
| 7 | PWMB |
| 8 | GND |
| 9 | VM |
| 10 | +3V3 |
| 11 | GND |
| 12 | M_L_P |
| 13 | M_L_N |
| 14 | M_R_N |
| 15 | M_R_P |
| 16 | GND |

### U3 — MPU9250_6500_MODULE_ASSUMED
2.54 mm pitch ASSUMED; AD0 low, NCS high, FSYNC low. Module VCC regulator/dropout must be checked; sensor identity unconfirmed.

| Pad | Net |
|---:|---|
| 1 | +3V3 |
| 2 | GND |
| 3 | SCL |
| 4 | SDA |
| 5 | NC_EDA |
| 6 | NC_ECL |
| 7 | GND |
| 8 | NC_INT |
| 9 | +3V3 |
| 10 | GND |

### J1 — LEFT_MOTOR_JST_PH_6_ASSUMED
2.0 mm pitch PLACEHOLDER, not validated JST housing. Cable pin order is proposed, not measured.

| Pad | Net |
|---:|---|
| 1 | M_L_N |
| 2 | M_L_P |
| 3 | +3V3 |
| 4 | GND |
| 5 | ENC_L_A |
| 6 | ENC_L_B |

### J2 — RIGHT_MOTOR_JST_PH_6_ASSUMED
Same assumptions as J1; encoder supply and logic must support 3.3 V.

| Pad | Net |
|---:|---|
| 1 | M_R_N |
| 2 | M_R_P |
| 3 | +3V3 |
| 4 | GND |
| 5 | ENC_R_A |
| 6 | ENC_R_B |

### J3 — EYE_INPUT_5PIN


| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |
| 3 | DIN_5V |
| 4 | CS_5V |
| 5 | CLK_5V |

### J4 — EYE_INPUT_5PIN


| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |
| 3 | CHAIN_DOUT |
| 4 | CS_5V |
| 5 | CLK_5V |

### J5 — EYE1_DOUT_RETURN
Connect eye 1 output DOUT here. J4 DIN gets this net, never ESP32 input.

| Pad | Net |
|---:|---|
| 1 | CHAIN_DOUT |
| 2 | GND |

### J6 — REAR_CAL_BUTTON
COM,NO,LED+,LED-. NC unused. Actual switch terminals must be identified.

| Pad | Net |
|---:|---|
| 1 | GND |
| 2 | CAL |
| 3 | +5V |
| 4 | GND |

### J7 — REGULATED_5V_INPUT
External regulated 5 V only; no raw battery. Fuse and reverse-polarity protection upstream.

| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |

### J8 — REGULATED_MOTOR_INPUT
External regulated nominal 3 V for stated 3 V motors. Supply/driver current suitability pending stall current.

| Pad | Net |
|---:|---|
| 1 | VM |
| 2 | GND |

### U4 — SN74AHCT125N_DIP14
PCB pad numbering is explicit DIP numbering. 5 V supply, three channels active, fourth disabled.

| Pad | Net |
|---:|---|
| 1 | GND |
| 2 | DISP_DIN |
| 3 | DIN_5V |
| 4 | GND |
| 5 | DISP_CS |
| 6 | CS_5V |
| 7 | GND |
| 14 | +5V |
| 13 | +5V |
| 12 | GND |
| 11 | NC_BUF_OUT |
| 10 | GND |
| 9 | DISP_CLK |
| 8 | CLK_5V |

### R1 — 10k
Encoder pull-ups populate only for verified compatible encoder outputs.

| Pad | Net |
|---:|---|
| 1 | ENC_L_A |
| 2 | +3V3 |

### R2 — 10k
Encoder pull-ups populate only for verified compatible encoder outputs.

| Pad | Net |
|---:|---|
| 1 | ENC_L_B |
| 2 | +3V3 |

### R3 — 10k
Encoder pull-ups populate only for verified compatible encoder outputs.

| Pad | Net |
|---:|---|
| 1 | ENC_R_A |
| 2 | +3V3 |

### R4 — 10k
Encoder pull-ups populate only for verified compatible encoder outputs.

| Pad | Net |
|---:|---|
| 1 | ENC_R_B |
| 2 | +3V3 |

### R5 — 10k
Encoder pull-ups populate only for verified compatible encoder outputs.

| Pad | Net |
|---:|---|
| 1 | CAL |
| 2 | +3V3 |

### R6 — 10k_STBY_PULLDOWN


| Pad | Net |
|---:|---|
| 1 | STBY |
| 2 | GND |

### C1 — 100nF


| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |

### C2 — 100nF


| Pad | Net |
|---:|---|
| 1 | +3V3 |
| 2 | GND |

### C3 — 100nF


| Pad | Net |
|---:|---|
| 1 | +3V3 |
| 2 | GND |

### C4 — 100nF


| Pad | Net |
|---:|---|
| 1 | VM |
| 2 | GND |

### C5 — 470uF_10V
Polarized: pad 1 positive; 8 mm diameter placeholder.

| Pad | Net |
|---:|---|
| 1 | VM |
| 2 | GND |

### C6 — 220uF_10V
Polarized: pad 1 positive; 8 mm diameter placeholder.

| Pad | Net |
|---:|---|
| 1 | +5V |
| 2 | GND |

### R7 — 4k7_DNP
Optional I2C pull-up; normally DNP if module already provides pull-ups to 3.3 V.

| Pad | Net |
|---:|---|
| 1 | SDA |
| 2 | +3V3 |

### R8 — 4k7_DNP
Optional I2C pull-up; normally DNP if module already provides pull-ups to 3.3 V.

| Pad | Net |
|---:|---|
| 1 | SCL |
| 2 | +3V3 |