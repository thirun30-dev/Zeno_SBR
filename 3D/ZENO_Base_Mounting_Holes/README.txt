ZENO BASE LAYER — REV B / PROTOTYPE
All dimensions mm. Designed for two N20 encoder motors, the supplied
76 x 41 x 19 battery holder, and nominal 44 diameter x 18 wide wheels.
This is the lower deck; top deck and motor caps are separate parts.

DIMENSIONS
Plate 124 across axle (X) x 96 fore/aft (Y) x 3 thick; corner radius 6.
Battery pocket internal: 41.8 X x 76.8 Y, centred on base.
Pocket wall 3 thick x 8 above plate; overall wall top Z=11.
Holder rests at Z=3; nominal holder top Z=22.
Wire notch 10 wide in +Y pocket wall, bottom Z=4; open upward.
Strap slots at X=+/-26,Y=0: 3 across X x 12 along Y, through plate.
Motor bracket holes: four diameter 3.2 THRU, X=+/-46,Y=+/-11.
Each bracket has 22 mm hole centre spacing along Y.
Left/right bracket centre separation is 92 mm along X.
Hole coordinates: (-46,-11),(-46,11),(46,-11),(46,11).
M3 clearance, not threaded; use washers and nuts beneath base.
Four top-deck standoff holes: diameter 3.2 at X=+/-52,Y=+/-38;
104 x 76 mounting pattern. M3 clearance, not tapped.
Cable passages at X=+/-39,Y=26: 12 along Y x 5 wide rounded slots.
Origin base centre, Z=0 underside; motor shafts lie along X, nominal Y=0.

MOTOR LAYOUT / LIMITS
Use user's modified N20_single_cap.step.SLDPRT as the intended motor cap.
Its exact geometry/hole positions could not be verified in this workflow.
The fixed 22 mm pitch follows the ORIGINAL single-cap design, not a verified
measurement of the modified SLDPRT. Check hole pitch and motor position in
SolidWorks before printing; these holes provide no slot adjustment.
No replacement motor cap is included. Trial-fit the modified cap before
printing the full deck, or overlay its STEP export on this base in SolidWorks.
Nominal motor location per side: gearbox front X=+/-63, gearbox rear +/-54,
rear motor/encoder envelope ends approximately +/-32. Shaft extends to +/-73.
Suggested steel casing clamp band: |X|=40 to 52, centred on bracket holes at 46.
These positions use the previous motor dimensional image; verify casing,
encoder and cap shape physically. Motor and wheel CAD are NOT included.
Keep wheel inner face outside |X|=64 for nominal 2 mm plate-edge clearance;
actual hub insertion and shaft engagement must be measured, not inferred from
44 x 18 wheel size. Do not operate with inadequate shaft engagement.
If shaft axis is approximately Z=9.1, nominal upright underside clearance is
22-9.1=12.9. With a 48 fore/aft half-length, base-ground contact occurs at
roughly 16 degrees pitch on level ground, ignoring fastener protrusions.
Actual axle height, loaded tyre radius and hub position remain unverified.
This is a geometry prototype, not a tested balancing-robot chassis.

ASSEMBLY
Use a <=10-wide nonconductive strap through pocket-side slots and over holder
so it cannot lift out. Keep the loop beneath the base clear of fasteners.
Route battery wires through +Y notch; two outer slots provide cable routing.
Use washers and nuts with M3 motor-cap bolts; lengths depend on modified cap.
Top-layer supports use four M3 standoffs on 104 x 76 centres. Select height
after top-deck component placement; holder top is already Z=22, so leave
additional clearance for cells, strap, wiring and service access.
Pocket and base share a single 3-thick floor; no stacked floor thickness.

PRINT / FILES
Suggested prototype PETG, bottom-down, 0.2 layers, 4 walls, 40-50% infill;
no supports normally required. Verify bed supports 124 x 96 footprint.
Printed clearance holes may need finishing. Test print mounting/pocket regions
or check parts in CAD before committing full print.
ZENO_base.step: single solid, importable into SolidWorks.
ZENO_base.stl: mm, print orientation bottom-down.
base_with_battery_envelope.step: base + simple 41 x 76 x 19 reference block;
block is not a detailed holder or a printable battery component.
layout.svg: dimensioned schematic top layout; use labelled values, not scaling.
preview.png: geometry preview.
build_base.py + battery_pocket_source.step: reproducible CadQuery source.
Validation: single valid solid, STEP/STL exported; physical fit not tested.
