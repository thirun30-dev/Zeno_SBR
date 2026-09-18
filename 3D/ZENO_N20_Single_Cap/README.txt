ZENO — SINGLE N20 MOTOR CAP / REV B
Units: mm. Single U-shaped part with two rounded tabs, following user reference.
Replaces the previous two-piece bracket. No separate lower clamp is required:
the flat chassis closes the open side and supports the motor.

DIMENSIONS
Overall: 30 wide x 12 along shaft x 14.9 high.
Internal rectangular channel: 10.4 wide x 12.4 high, open both axial ends.
Wall and roof thickness: 2.5.
Mounting tab thickness: 3.0; rounded end radius 4.0.
Two M3 clearance holes: diameter 3.2 THRU, 22 centre-to-centre.
Hole coordinates: X=-11 and +11, Y=0. Shaft direction is Y.
Chassis contact plane: Z=0. Channel ceiling: Z=12.4.

FIT ASSUMPTIONS — CHECK BEFORE PRINTING
User motor drawing shows 10 x 12 gearbox face, 12.2 max body diameter,
9 gearbox length and 22 +/-1 motor/encoder rear envelope.
The motor casing width across flats is not separately specified. Model assumes
10.0 across flats plus 0.4 total clearance. Height uses 12.2 maximum body
height plus 0.2 clearance. Check actual casing width and height with calipers.
The 12-long cap must fit entirely over steel motor casing, clear of exposed
gears, rear endbell, encoder, connector and any protrusions.
This is a geometrically validated prototype, not a physically tested fit.

INSTALLATION
Place motor casing on chassis with its narrow (assumed 10 mm) dimension
across the cap. Install cap over it, rounded tabs down against chassis.
Use a thin compliant liner/shim to eliminate clearance and create a snug grip.
Do not expect the oversized bare channel alone to clamp the motor securely.
Select liner thickness by actual fit; cap should grip before tabs fully seat.
Use two M3 bolts, washers and nuts; bolt length depends on chassis thickness.
For 3 mm chassis, M3x12 is a starting option: verify washer/nut stack and
thread engagement. Snug evenly without deforming motor casing or tabs.
Confirm motor cannot slide or rotate under operating torque.
Print one cap and test fit before producing the second robot motor mount.

FABRICATION AND FILES
Suggested prototype material PETG; layer 0.2; 4 walls; 50% infill.
STL is oriented roof-down / opening-up, resembling reference photo.
Support is required beneath elevated mounting tabs in this orientation.
Clean supports and finish M3 clearance holes with a 3.2 mm drill if necessary.
STEP uses installed coordinates: tabs at bottom, opening toward chassis.
N20_single_cap.step: solid CAD model importable into SolidWorks.
N20_single_cap.stl: print-oriented mesh in mm.
N20_single_cap.svg: CAD view, not a scale drawing.
preview.png: inverted view exposing internal channel.
build_cap.py: editable CadQuery generator, with explicit fit parameters.
