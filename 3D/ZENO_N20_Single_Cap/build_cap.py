from pathlib import Path
import cadquery as cq
p=Path(__file__).parent
# mm; chassis at Z=0; shaft parallel to Y.
inner_w=10.4  # assumes measured motor across flats 10.0 + 0.4 clearance
inner_h=12.4 # accommodates drawing's 12.2 max body diameter + 0.2
wall=2.5
depth=12.0
ear_t=3.0
pitch=22.0
outer_w=inner_w+2*wall
height=inner_h+wall
s=cq.Workplane('XY').box(outer_w,depth,height,centered=(True,True,False))
s=s.cut(cq.Workplane('XY').box(inner_w,depth+2,inner_h+1,centered=(True,True,False)).translate((0,0,-1)))
for side in [-1,1]:
 x=side*pitch/2
 ear=cq.Workplane('XY').center(x,0).circle(4).extrude(ear_t)
 bridge=cq.Workplane('XY').box(4.5,8,ear_t,centered=(True,True,False)).translate((side*9,0,0))
 s=s.union(ear).union(bridge)
 s=s.cut(cq.Workplane('XY').center(x,0).circle(1.6).extrude(ear_t+1))
assert s.val().isValid() and len(s.solids().vals())==1
cq.exporters.export(s,str(p/'N20_single_cap.step'))
# Print roof-down: U opens upward. Tabs then need support beneath them.
pr=s.rotate((0,0,0),(1,0,0),180).translate((0,0,height))
cq.exporters.export(pr,str(p/'N20_single_cap.stl'))
cq.exporters.export(s,str(p/'N20_single_cap.svg'),opt={'width':1000,'height':700,'projectionDir':(1,-1,0.8),'showHidden':False})
# Exact geometry preview, shown upside down to expose U channel like reference.
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
v,t=pr.val().tessellate(0.12)
verts=[[(v[i].x,v[i].y,v[i].z) for i in tri] for tri in t]
fig=plt.figure(figsize=(9,6),facecolor='#f1f5f9');ax=fig.add_subplot(111,projection='3d')
ax.add_collection3d(Poly3DCollection(verts,facecolors='#b7d8ed',shade=True,lightsource=matplotlib.colors.LightSource(azdeg=300,altdeg=45)))
ax.set_xlim(-16,16);ax.set_ylim(-8,8);ax.set_zlim(0,16);ax.set_box_aspect((32,16,16));ax.view_init(elev=28,azim=-55);ax.set_axis_off();ax.set_facecolor('#f1f5f9')
fig.suptitle('ZENO • N20 single mounting cap',fontsize=19,y=.94)
fig.text(.5,.07,'Two Ø3.2 M3 holes • 22 mm centres • 30 × 12 × 14.9 mm',ha='center',fontsize=12)
fig.text(.5,.025,'Shown inverted to reveal channel. Motor fit requires verification.',ha='center',fontsize=10)
fig.savefig(p/'preview.png',dpi=160,bbox_inches='tight');plt.close(fig)
print('Valid single solid; STEP/STL exported.',s.val().BoundingBox().xlen,s.val().BoundingBox().ylen,s.val().BoundingBox().zlen)
