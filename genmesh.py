import bpy
import os
dir_path=os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
#new_path=os.path.dirname(os.path.join(dir_path,'..\..'))
new_path=os.path.join(dir_path,'meshMeteorito.dat')
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
for polygon in polygons:
    verts=polygon.vertices;
    verts=[verts[0],verts[2],verts[1]];
    for vert in verts:
        F.write('%d\n' %(vert));
F.close();
print("Finalizada la escritua de los datos");