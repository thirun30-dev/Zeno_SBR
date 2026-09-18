from pathlib import Path
import cadquery as cq
p=Path(__file__).parent
# mm. X wheel axle, Y fore/aft, Z up. Bottom of base Z=0.
base=cq.Workplane('XY').box(124,96,3,centered=(True,True,False)).edges('|Z').fillet(6)
tray=cq.importers.importStep(str(p/'battery_pocket_source.step')).rotate((0,0,0),(0,0,1),90)
base=base.union(tray)
# Restore strap slots through shared base floor after union.
for x in [-26,26]:
 base=base.cut(cq.Workplane('XY').center(x,0).rect(3,12).extrude(4))
# Motor clamp holes: M3 clearance, original cap 22 mm pitch.
for x in [-46,46]:
 for y in [-11,11]:
  base=base.cut(cq.Workplane('XY').center(x,y).circle(1.6).extrude(4))
# Top-deck standoffs: 104 x 76 pattern.
for x in [-52,52]:
 for y in [-38,38]:
  base=base.cut(cq.Workplane('XY').center(x,y).circle(1.6).extrude(4))
# Encoder / power cable passages to underside, away from motor band.
for x in [-39,39]:
 base=base.cut(cq.Workplane('XY').center(x,26).slot2D(12,5,90).extrude(4))
assert base.val().isValid() and len(base.solids().vals())==1
cq.exporters.export(base,str(p/'ZENO_base.step'))
cq.exporters.export(base,str(p/'ZENO_base.stl'))
# Nominal rectangular reference envelope only.
env=cq.Workplane('XY').box(41,76,19,centered=(True,True,False)).translate((0,0,3))
a=cq.Assembly();a.add(base,name='base',color=cq.Color(.12,.42,.7));a.add(env,name='battery_holder_ENVELOPE',color=cq.Color(.2,.2,.2,.4));a.save(str(p/'base_with_battery_envelope.step'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
v,t=base.val().tessellate(.15)
polys=np.array([[(v[i].x,v[i].y,v[i].z) for i in tri] for tri in t]);n=np.cross(polys[:,1]-polys[:,0],polys[:,2]-polys[:,0]);n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-12)
light=np.array([.3,-.5,.8]);light/=np.linalg.norm(light);cols=np.array([.4,.66,.85])[None,:]*(.55+.45*np.abs(n@light))[:,None]
fig=plt.figure(figsize=(11,7),facecolor='#f1f5f9');ax=fig.add_subplot(111,projection='3d');ax.set_facecolor('#f1f5f9');ax.add_collection3d(Poly3DCollection(polys,facecolors=cols,edgecolors=cols,linewidths=.08));ax.set_xlim(-65,65);ax.set_ylim(-50,50);ax.set_zlim(0,15);ax.set_box_aspect((130,100,15));ax.view_init(42,-55);ax.set_axis_off();fig.suptitle('ZENO | Base layer — Rev B',fontsize=21,y=.93);fig.text(.5,.07,'124 × 96 mm • 3 mm plate • Integrated battery pocket',ha='center',fontsize=13);fig.text(.5,.03,'Four M3 motor bracket holes • Four M3 standoff holes • Strap and cable slots',ha='center',fontsize=11);fig.savefig(p/'preview.png',dpi=150,bbox_inches='tight')
print('Valid single solid; STEP and STL exported.')
