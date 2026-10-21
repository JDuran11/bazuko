# Junta las animaciones descargadas de Mixamo (FBX, "Without Skin") en un solo GLB para el juego.
# Cada archivo se vuelve un clip con el nombre del archivo (sin extensión): idle.fbx → "idle".
#
# Uso:
#   blender -b --python tools/personaje/mixamo_a_glb.py -- <carpeta_con_fbx> public/models/animaciones.glb
import os
import sys
import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 2:
    raise SystemExit("Uso: blender -b --python mixamo_a_glb.py -- <carpeta_fbx> <salida.glb>")
SRC, OUT = argv[0], argv[1]

files = sorted(f for f in os.listdir(SRC) if f.lower().endswith(".fbx"))
if not files:
    raise SystemExit(f"No hay archivos .fbx en {SRC}")

bpy.ops.wm.read_factory_settings(use_empty=True)

main_rig = None
for i, fname in enumerate(files):
    clip = os.path.splitext(fname)[0].strip().lower().replace(" ", "_")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(SRC, fname), automatic_bone_orientation=False)
    new = [o for o in bpy.data.objects if o not in before]
    rig = next(o for o in new if o.type == 'ARMATURE')
    action = rig.animation_data.action if rig.animation_data else None
    if action is None:
        print("SIN ANIMACION", fname)
        continue
    action.name = clip
    action.use_fake_user = True

    if main_rig is None:
        main_rig = rig
        main_rig.name = "mixamo"
        # el primer clip también va como pista NLA para que el exportador lo incluya
        main_rig.animation_data.action = None
    else:
        for o in new:
            bpy.data.objects.remove(o, do_unlink=True)

    track = main_rig.animation_data.nla_tracks.new()
    track.name = clip
    strip = track.strips.new(clip, int(action.frame_range[0]), action)
    strip.name = clip
    track.mute = True
    print("CLIP", clip, f"{action.frame_range[1] - action.frame_range[0]:.0f} frames")

# Solo el esqueleto (las animaciones vienen sin piel)
for o in list(bpy.data.objects):
    if o.type == 'MESH':
        bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.export_scene.gltf(
    filepath=OUT,
    export_format='GLB',
    export_animations=True,
    export_animation_mode='NLA_TRACKS',
    export_force_sampling=True,
    export_frame_step=1,
    export_yup=True,
)
print("EXPORTED", OUT, len(files), "clips")
