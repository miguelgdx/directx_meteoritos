import bpy
import mathutils
from mathutils import Vector

verts=[Vector((-1.0,-1.0,1.0)),
    Vector((1.0,-1.0,1.0)),
    Vector((1.0,-1.0,-1.0)),
    Vector((-1.0,-1.0,-1.0)),
    Vector((-1.0,1.0,1.0)),
    Vector((1.0,1.0,1.0)),
    Vector((1.0,1.0,-1.0)),
    Vector((-1.0,1.0,-1.0))];
    
faces=[(0,3,2),(0,2,1),(4,6,7),(4,5,6),(3,4,7),(3,0,4),(0,1,4),(4,1,5),
    (1,6,5),(1,2,6),(3,7,6),(3,6,2)];
    
mesh=bpy.data.meshes.new(name='Basic')
mesh.from_pydata(verts,[],faces)
for v in mesh.vertices:
    print(str(v.co)+" "+str(v.normal))
obj=bpy.data.objects.new('Basic',mesh)
bpy.context.scene.collection.objects.link(obj);
# bpy.context.scene.objects.link(obj); # Para versión 2.7 de Blender