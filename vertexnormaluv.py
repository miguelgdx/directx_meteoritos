import bpy
import os
dir_path=os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
#new_path=os.path.dirname(os.path.join(dir_path,'..\..'))
new_path=os.path.join(dir_path,'mesh2.dat')
print(new_path)
F=open(new_path,"w")
sobjects=bpy.context.selected_objects;
object=sobjects[0];
object_data=object.data;
vertices=object_data.vertices;
F.write('%s\n' % (len(vertices)));
for vert in vertices:
    coord=vert.co;
    F.write('%f %f %f\n' % (coord[0],coord[2],coord[1]))
for vert in vertices:
    normal=vert.normal;
    F.write('%f %f %f\n' % (normal[0],normal[2],normal[1]))
polygons=object_data.polygons;
nindices=len(polygons)*3;
F.write('%d\n' % nindices);
uvs=[(0.0,0.0)]*len(vertices);
for polygon in polygons:
    verts=polygon.vertices;
    loops=polygon.loop_indices;
    for loopindex in loops:
        loop = object_data.loops[loopindex];
        uvloop=object_data.uv_layers.active.data[loopindex];
        print(uvloop.uv);
        uvs[loop.vertex_index]=uvloop.uv;
    verts=[verts[0],verts[2],verts[1]];
    
    for vert in verts:
        F.write('%d\n' %(vert));
print(uvs);
for uv in uvs:
    F.write('%f %f\n' % (uv[0],uv[1]));
F.close();
print("Finalizada la escritua de los datos");