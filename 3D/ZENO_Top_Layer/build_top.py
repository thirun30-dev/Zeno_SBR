from pathlib import Path
import cadquery as cq
p=Path(__file__).parent
# mm. Top deck local underside Z=0; installed underside Z=35.
def box(w,l,h,z=0,x=0,y=0):
 return cq.Workplane('XY').box(w,l,h,centered=(True,True,False)).translate((x,y,z))
deck=box(144,100,3).edges('|Z').fillet(6)
# Four integral PCB bosses; assumed PCB holes 5 mm inward from each edge.
for x in [-60,60]:
 for y in [-33.5,33.5]:
  deck=deck.union(cq.Workplane('XY').center(x,y).circle(4).extrude(11))
  deck=deck.cut(cq.Workplane('XY').center(x,y).circle(1.6).extrude(12))
# Inter-deck fasteners match existing base 104 x 76 hole pattern.
for x in [-52,52]:
 for y in [-38,38]:
  deck=deck.cut(cq.Workplane('XY').center(x,y).circle(1.6).extrude(4))
# Cable access outside PCB outline (+Y edge); no guessed display holes.
for x in [-25,25]:
 deck=deck.cut(cq.Workplane('XY').center(x,44).slot2D(18,5,0).extrude(4))
spacer=cq.Workplane('XY').circle(5).circle(1.6).extrude(32)
for name,obj in [('top_deck',deck),('deck_spacer_32mm',spacer)]:
 assert obj.val().isValid() and len(obj.solids().vals())==1
 cq.exporters.export(obj,str(p/f'{name}.step'));cq.exporters.export(obj,str(p/f'{name}.stl'))
# Installed layout: base top3 + spacer32 => top deck underside35.
a=cq.Assembly();a.add(deck.translate((0,0,35)),name='top_deck',color=cq.Color(.16,.45,.75))
base=cq.importers.importStep(str(p/'base_reference.step'));a.add(base,name='base_reference',color=cq.Color(.25,.4,.52))
for i,(x,y) in enumerate([(x,y) for x in [-52,52] for y in [-38,38]]):a.add(spacer.translate((x,y,3)),name=f'frame_spacer_{i+1}',color=cq.Color(.7,.7,.72))
pcb=box(130,77,5,z=46)
a.add(pcb,name='PCB_ENVELOPE_130x77x5',color=cq.Color(.05,.48,.21,.6))
for i,x in enumerate([-22,22]):
 a.add(box(32,51,14,z=51,x=x),name=f'DISPLAY_ENVELOPE_{i+1}_32x51x14',color=cq.Color(.15,.15,.15,.5))
a.save(str(p/'two_layer_layout.step'))
# Preview manufactured top deck and four loose spacers (exploded layout).
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
fig=plt.figure(figsize=(10,7),facecolor='#f1f5f9');ax=fig.add_subplot(111,projection='3d');ax.set_facecolor('#f1f5f9')
def draw(obj,color):
 v,t=obj.val().tessellate(.15);q=np.array([[(v[i].x,v[i].y,v[i].z) for i in tri] for tri in t]);n=np.cross(q[:,1]-q[:,0],q[:,2]-q[:,0]);n/=np.maximum(np.linalg.norm(n,axis=1)[:,None],1e-12);light=np.array([.3,-.5,.8]);light/=np.linalg.norm(light);cols=np.array(color)[None,:]*(.55+.45*np.abs(n@light))[:,None];ax.add_collection3d(Poly3DCollection(q,facecolors=cols,edgecolors=cols,linewidths=.08))
draw(deck.translate((0,0,38)),[.4,.66,.85])
for x in [-52,52]:
 for y in [-38,38]:draw(spacer.translate((x,y,0)),[.7,.73,.77])
ax.set_xlim(-76,76);ax.set_ylim(-54,54);ax.set_zlim(0,52);ax.set_box_aspect((152,108,52));ax.view_init(35,-55);ax.set_axis_off();fig.suptitle('ZENO | Top deck with four PCB standoffs',fontsize=20,y=.95);fig.text(.5,.075,'144 × 100 × 3 mm deck • PCB supports: Ø8 × 8 mm above deck',ha='center',fontsize=12);fig.text(.5,.035,'Four 32 mm frame spacers shown below • PCB hole pattern assumed: 120 × 67 mm',ha='center',fontsize=11);fig.savefig(p/'preview.png',dpi=150,bbox_inches='tight');print('Validated deck and spacer solids; exported STEP/STL and assembly.')
