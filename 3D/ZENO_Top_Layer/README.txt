ZENO TOP LAYER — REV A / PROVISIONAL PCB HOLE PATTERN
All dimensions mm. This design adds a top deck, four integral PCB standoffs,
and four separate inter-deck spacers matching the existing base.

USER DIMENSIONS
PCB: 130 x 77 x 5. Treated as a rectangular envelope, not a verified board
outline with traces, underside components or measured mounting holes.
Two complete dot-matrix modules: each 51 x 32 x 14.
Separate user value 32 x 32 x 80 conflicts with the complete module envelope.
It has NOT been silently corrected to 8.0. Confirm that value before making
display recesses, face panels or mounting brackets.

TOP DECK
144 along axle X x 100 fore/aft Y x 3 thick; corner radius 6.
Four integral PCB supports: diameter 8, height 8 above deck top.
Support holes diameter 3.2 THRU support and plate, for M3 bolts/nuts.
PCB hole coordinates X=+/-60,Y=+/-33.5, pattern 120 x 67.
THIS PATTERN IS ASSUMED: holes inset 5 from each PCB edge. PCB dimensions
alone do not establish actual mounting holes. Verify or edit source before
fabrication. Do not drill an assembled PCB without checking copper/components.
Four frame attachment holes diameter 3.2 at X=+/-52,Y=+/-38,
pattern 104 x 76 matching the base layer.
Two cable slots 18 x 5 along X, centred X=+/-25,Y=44, outside PCB footprint.
Origin deck centre, local underside Z=0. Local support top Z=11.

FRAME SPACERS
Print 4 copies of deck_spacer_32mm.stl.
Each spacer diameter 10, length 32, through bore diameter 3.2.
These are hollow spacers for through bolts, not threaded pillars.
Use four M3 bolts passing through base, spacer and top deck, with washers/nuts.
Plate/spacer stack is 3+32+3=38; M3x45 is a starting length; verify actual
washer/nut stack and thread protrusion. Assemble frame before installing PCB.
PCB mounting stack: deck3 + support8 + PCB5 =16; M3x22 with washers/nuts is
an initial option. Confirm actual board thickness and underside clearance.
Use insulating washers as needed for the actual board layout.

INSTALLED HEIGHTS — ALL FROM BASE UNDERSIDE
Base top: Z=3.
Battery-holder envelope top: Z=22.
Top deck underside: Z=35; top surface Z=38.
PCB underside on supports: Z=46; envelope top Z=51.
Two flat display reference blocks: Z=51 to65; placement is illustrative only.
Battery-holder envelope-to-deck clearance: 13; verify installed cells, strap,
wires and access for removal. Component pin heights are not supplied.
Nominal 44 mm wheel top from prior assumed axle Z=9.1: Z=31.1.
Top deck underside has nominal 3.9 clearance above wheel; actual tyre, hub,
axle, fastener protrusions and motor cap geometry require assembly checking.

DISPLAY PLACEMENT
Assembly shows two modules flat, 32 along X x 51 along Y x 14 high, centred
X=-22 and +22,Y=0, with 12 between module envelopes. Their physical fasteners,
connectors, cables and other PCB components are not modelled. These are NOT
verified mounts or detailed module CAD. No display-specific holes were guessed.
PCB can be populated differently after actual component layout is established.

FILES / PRINTING
1 x top_deck.stl: print underside-down; 4 PCB supports are integral.
4 x deck_spacer_32mm.stl: print upright.
STEP files: same solid geometry, importable into SolidWorks.
two_layer_layout.step: base reference + top deck + four frame spacers + simple
PCB and two display envelopes. Envelopes are NOT parts to print.
base_reference.step: copy of previous base for assembly reference only.
build_top.py: CadQuery generator; pattern is explicit and editable.
layout.svg: dimensioned schematic top view and height schedule.
preview.png: top deck and frame spacers in an exploded view.
Suggested prototype PETG, 0.2 layers, 4 walls, 40-50% infill; spacers 100%.
No supports normally required. Finish clearance bores if printer undersizes.
Validation: deck and spacer are single valid solids; exported to STEP/STL.
Physical fit, PCB layout, display mounts and loaded rigidity remain untested.
