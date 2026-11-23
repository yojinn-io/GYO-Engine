# Makes skinned_bar.fbx, the model loader's small contract example
# (docs/architecture/target/v2/model.zh-Hant.md section 3), from nothing, with
# Blender's own FBX exporter. Run it from the repository root:
#
#     Blender --background --factory-startup --python tests/common/model/contract/make_skinned_bar.py
#
# The scene, in Blender's units (metres, +Z up):
# - an armature "Rig" with bone "Lower" from (0, 0, 0) to (0, 0, 1), its
#   child "Upper" from (0, 0, 1) to (0, 0, 2), and three more children of
#   Lower that never move, "Aux1", "Aux2", "Aux3", beside it at x = 1;
# - a mesh "Bar": a box 0.2 wide and deep, from z = 0 to z = 2, cut at z = 1;
#   vertices below z = 1 follow Lower (weight 1), above it Upper (weight 1),
#   at z = 1 both with weight 0.3 each (not summing to 1);
# - a mesh "Tag" with no material: one triangle at x = 1; its first vertex has
#   five influences, Lower 0.4, Upper 0.3, Aux1 0.2, Aux2 0.2, Aux3 0.1, the
#   other two Lower alone;
# - a material "Paint", base colour (0.8, 0.1, 0.1, 1), its base colour from an
#   image named textures/paint.png (the file is not written: a loader only
#   names it);
# - UVs: the bottom face's corners at v = 0, the top face's at v = 1;
# - two actions as separate stacks, 30 frames a second: "Bend", Upper turning
#   90 degrees about its own X from frame 1 to frame 31 (1 second); "Hold",
#   nothing moving, frames 1 to 16 (0.5 seconds).
import bpy
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "skinned_bar.fbx")

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = 30
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

# Armature.
armature_data = bpy.data.armatures.new("Rig")
rig = bpy.data.objects.new("Rig", armature_data)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
lower = armature_data.edit_bones.new("Lower")
lower.head = (0.0, 0.0, 0.0)
lower.tail = (0.0, 0.0, 1.0)
upper = armature_data.edit_bones.new("Upper")
upper.head = (0.0, 0.0, 1.0)
upper.tail = (0.0, 0.0, 2.0)
upper.parent = lower
upper.use_connect = True
for i, name in enumerate(["Aux1", "Aux2", "Aux3"]):
    aux = armature_data.edit_bones.new(name)
    aux.head = (1.0, 0.0, 0.2 * i)
    aux.tail = (1.0, 0.0, 0.2 * i + 0.1)
    aux.parent = lower
bpy.ops.object.mode_set(mode='OBJECT')

# Mesh: two stacked boxes sharing the middle ring.
h = 0.1
rings = [0.0, 1.0, 2.0]
verts = []
for z in rings:
    verts += [(-h, -h, z), (h, -h, z), (h, h, z), (-h, h, z)]
faces = [(0, 3, 2, 1), (8, 9, 10, 11)]  # bottom, top
for r in range(2):
    b = r * 4
    for i in range(4):
        j = (i + 1) % 4
        faces.append((b + i, b + j, b + 4 + j, b + 4 + i))
mesh_data = bpy.data.meshes.new("Bar")
mesh_data.from_pydata(verts, [], faces)
mesh_data.update()
bar = bpy.data.objects.new("Bar", mesh_data)
scene.collection.objects.link(bar)

uv = mesh_data.uv_layers.new(name="UVMap")
for poly in mesh_data.polygons:
    for loop_index in poly.loop_indices:
        z = mesh_data.vertices[mesh_data.loops[loop_index].vertex_index].co.z
        x = mesh_data.vertices[mesh_data.loops[loop_index].vertex_index].co.x
        uv.data[loop_index].uv = (0.0 if x < 0 else 1.0, z / 2.0)

image = bpy.data.images.new("paint", 4, 4)
image.filepath_raw = "//textures/paint.png"
image.source = 'FILE'
material = bpy.data.materials.new("Paint")
material.use_nodes = True
bsdf = material.node_tree.nodes["Principled BSDF"]
bsdf.inputs["Base Color"].default_value = (0.8, 0.1, 0.1, 1.0)
texture_node = material.node_tree.nodes.new("ShaderNodeTexImage")
texture_node.image = image
material.node_tree.links.new(texture_node.outputs["Color"], bsdf.inputs["Base Color"])
mesh_data.materials.append(material)

lower_group = bar.vertex_groups.new(name="Lower")
upper_group = bar.vertex_groups.new(name="Upper")
for index, (_, _, z) in enumerate(verts):
    if z < 0.5:
        lower_group.add([index], 1.0, 'REPLACE')
    elif z > 1.5:
        upper_group.add([index], 1.0, 'REPLACE')
    else:
        lower_group.add([index], 0.3, 'REPLACE')
        upper_group.add([index], 0.3, 'REPLACE')
bar.parent = rig
modifier = bar.modifiers.new("Armature", 'ARMATURE')
modifier.object = rig

tag_data = bpy.data.meshes.new("Tag")
tag_data.from_pydata([(1.0, 0.0, 0.5), (1.2, 0.0, 0.5), (1.0, 0.0, 0.7)], [], [(0, 1, 2)])
tag_data.update()
tag = bpy.data.objects.new("Tag", tag_data)
scene.collection.objects.link(tag)
for name, weight in [("Lower", 0.4), ("Upper", 0.3), ("Aux1", 0.2), ("Aux2", 0.2), ("Aux3", 0.1)]:
    tag.vertex_groups.new(name=name).add([0], weight, 'REPLACE')
tag.vertex_groups["Lower"].add([1, 2], 1.0, 'REPLACE')
tag.parent = rig
tag_modifier = tag.modifiers.new("Armature", 'ARMATURE')
tag_modifier.object = rig

# Actions.
rig.animation_data_create()
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='POSE')
pose_upper = rig.pose.bones["Upper"]
pose_upper.rotation_mode = 'XYZ'


def make_action(name, frames, end_degrees):
    action = bpy.data.actions.new(name)
    rig.animation_data.action = action
    pose_upper.rotation_euler = (0.0, 0.0, 0.0)
    pose_upper.keyframe_insert("rotation_euler", frame=1)
    pose_upper.rotation_euler = (end_degrees * 3.141592653589793 / 180.0, 0.0, 0.0)
    pose_upper.keyframe_insert("rotation_euler", frame=frames)
    for fcurve in action.fcurves:
        for point in fcurve.keyframe_points:
            point.interpolation = 'LINEAR'
    track = rig.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, 1, action)
    rig.animation_data.action = None
    return action


make_action("Bend", 31, 90.0)
make_action("Hold", 16, 0.0)
bpy.ops.object.mode_set(mode='OBJECT')
pose_upper.rotation_euler = (0.0, 0.0, 0.0)
scene.frame_start = 1
scene.frame_end = 31

bpy.ops.export_scene.fbx(
    filepath=OUT,
    use_selection=False,
    object_types={'ARMATURE', 'MESH'},
    add_leaf_bones=False,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False,
    bake_anim_force_startend_keying=True,
    path_mode='RELATIVE',
    embed_textures=False,
    use_mesh_modifiers=False,
)
print("wrote", OUT)
