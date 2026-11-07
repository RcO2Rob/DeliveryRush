"""Run INSIDE Unreal Editor, after building the DeliveryRush module.

Creates native editable .umap/.uasset files using engine primitives only.
Python is used for editor authoring; gameplay is entirely in the C++ module.
Existing finished maps and materials are preserved on subsequent runs.
"""
import json
import hashlib
import math
import random
import struct
import wave
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
LAYOUT = json.loads((ROOT / "Design/city_layout.json").read_text())
MAP = LAYOUT["map"]
MARKER = ROOT / "Content" / LAYOUT["marker"]
ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

PALETTE = {
    "Ink": (0.018, 0.025, 0.036), "Navy": (0.035, 0.075, 0.12),
    "Silver": (0.50, 0.60, 0.65), "Coral": (0.83, 0.13, 0.09),
    "Teal": (0.035, 0.55, 0.43), "Amber": (0.95, 0.48, 0.06),
    "Ivory": (0.88, 0.85, 0.72), "Skin": (0.62, 0.32, 0.17),
    "Road": (0.065, 0.085, 0.10), "Pavement": (0.32, 0.36, 0.37),
    "Ground": (0.12, 0.19, 0.18), "Sage": (0.24, 0.38, 0.32),
    "Brick": (0.44, 0.25, 0.20), "Sand": (0.55, 0.45, 0.31),
    "Window": (0.10, 0.22, 0.27), "GlowRed": (1.0, 0.04, 0.025),
    "GlowAmber": (1.0, 0.58, 0.10), "GlowTeal": (0.04, 0.95, 0.60),
    "Grass": (0.13, 0.31, 0.065), "GrassLight": (0.24, 0.43, 0.09),
    "Dirt": (0.38, 0.25, 0.12), "Bark": (0.19, 0.10, 0.045),
}


def static_mesh_usage(mat):
    # UE 5.8's local vertex factory requires StaticMesh usage even for the
    # procedural terrain. Persist this flag so standalone uses the real shader.
    usage=unreal.MaterialUsage.MATUSAGE_STATIC_MESH
    unreal.MaterialEditingLibrary.set_base_material_usage(mat,usage,True)
    unreal.MaterialEditingLibrary.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NANITE,True)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    if not unreal.MaterialEditingLibrary.has_material_usage(mat,usage):
        raise RuntimeError("StaticMesh material usage was not enabled: "+mat.get_name())
    # Loading an old material can auto-enable a usage flag without leaving its
    # package dirty. Force-save the actual flag instead of trusting that state.
    if not ASSETS.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError("Could not save material usage: "+mat.get_name())
    return mat


def material(name, rgb):
    path = "/Game/Art/M_" + name
    if ASSETS.does_asset_exist(path):
        return static_mesh_usage(ASSETS.load_asset(path))
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + name, "/Game/Art", unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionVectorParameter, -320, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(*rgb, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -320, 140)
    roughness.set_editor_property("r", 0.82)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if name.startswith("Glow"):
        unreal.MaterialEditingLibrary.connect_material_property(
            color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    if not ASSETS.save_loaded_asset(mat):
        raise RuntimeError("Could not save material: " + path)
    return static_mesh_usage(mat)


def audio():
    folder = ROOT / "Saved" / "GeneratedAudio"
    folder.mkdir(parents=True, exist_ok=True)
    sample_rate = 22050
    definitions = {
        "Horn": [(440, .15), (0, .07), (554, .22)],
        "Hit": [(95, .11), (60, .16)],
        "Checkpoint": [(523, .09), (784, .15)],
        "Delivery": [(523, .12), (659, .12), (784, .12), (1047, .35)],
    }
    for name, notes in definitions.items():
        asset = "/Game/Audio/S_" + name
        if ASSETS.does_asset_exist(asset):
            continue
        samples = []
        for frequency, duration in notes:
            count = int(sample_rate * duration)
            for index in range(count):
                t = index / sample_rate
                envelope = min(1.0, index / 220) * min(1.0, (count - index) / 900)
                value = (math.sin(2 * math.pi * frequency * t)
                         + .22 * math.sin(4 * math.pi * frequency * t)) if frequency else 0
                samples.append(struct.pack("<h", int(12500 * envelope * value)))
        source = folder / (name + ".wav")
        with wave.open(str(source), "wb") as out:
            out.setnchannels(1)
            out.setsampwidth(2)
            out.setframerate(sample_rate)
            out.writeframes(b"".join(samples))
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", "/Game/Audio")
        task.set_editor_property("destination_name", "S_" + name)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if not ASSETS.does_asset_exist(asset):
            raise RuntimeError("Audio import failed: " + asset)


def vector(x, y, z):
    return unreal.Vector(x * 100, y * 100, z * 100)


def build_map(materials):
    if ASSETS.does_asset_exist(MAP):
        if not MARKER.exists():
            raise RuntimeError(
                MAP + " already exists without a completed bootstrap marker. "
                "It may contain edits or a partial build. Back up/rename it in the "
                "editor before regenerating; this script will not overwrite it.")
        marker = json.loads(MARKER.read_text())
        current_hash = hashlib.sha256((ROOT / "Design/city_layout.json").read_bytes()).hexdigest()
        if marker.get("layout_sha256") != current_hash or marker.get("map") != MAP:
            raise RuntimeError("Layout differs from the saved map. Preserve it and author a new map before updating collision.")
        unreal.log("Preserving existing completed map: " + MAP)
        if not LEVELS.load_level(MAP):
            raise RuntimeError("Could not load terrain map for visual baking")
        terrain=next(a for a in ACTORS.get_all_level_actors() if isinstance(a,unreal.DeliveryTerrain))
        mesh=terrain.bake_surface()
        unreal.AutomationLibrary.finish_loading_before_screenshot()
        if not mesh or not ASSETS.save_loaded_asset(mesh,only_if_is_dirty=False) or not LEVELS.save_current_level():
            raise RuntimeError("Could not save baked terrain visual")
        return
    if MAP == "/Game/Maps/DeliveryEditable":
        import runpy
        runpy.run_path(str(ROOT / "tools/migrate_editable_map.py"))
        return
    if not LEVELS.new_level(MAP):
        raise RuntimeError("Could not create " + MAP)
    meshes = {name: ASSETS.load_asset("/Engine/BasicShapes/" + name)
              for name in ("Cube", "Cylinder", "Sphere")}

    def shape(label, loc, size, color, kind="Cube", rotation=None, ground=True):
        if ground:
            loc=(loc[0],loc[1],loc[2]+unreal.DeliveryTerrain.get_ground_height(loc[0],loc[1]))
        actor = ACTORS.spawn_actor_from_class(
            unreal.StaticMeshActor, vector(*loc), rotation or unreal.Rotator())
        actor.set_actor_label(label)
        component = actor.static_mesh_component
        component.set_static_mesh(meshes[kind])
        component.set_material(0, materials[color])
        actor.set_actor_scale3d(unreal.Vector(*size))
        actor.set_folder_path("City/" + label.split("_")[0])
        return actor

    def sign(label, text, loc, color="Ivory", height=.75):
        actor = ACTORS.spawn_actor_from_class(
            unreal.TextRenderActor, vector(*loc), unreal.Rotator(pitch=0,yaw=180,roll=0))
        actor.set_actor_label(label)
        component = actor.text_render
        component.set_text(text)
        component.set_world_size(height * 100)
        rgb = PALETTE[color]
        component.set_text_render_color(unreal.Color(
            int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255), 255))
        component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        actor.set_folder_path("City/Signs")

    terrain=ACTORS.spawn_actor_from_class(unreal.DeliveryTerrain,vector(0,0,0))
    terrain.set_actor_label("Terrain_Hills")
    terrain.set_folder_path("City/Terrain")
    mesh=terrain.bake_surface()
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    if not mesh or not ASSETS.save_loaded_asset(mesh,only_if_is_dirty=False):
        raise RuntimeError("Could not save baked terrain visual")
    axes=LAYOUT["street_axes"]
    split=LAYOUT["park_min_x"]
    for idx, prop in enumerate(LAYOUT["obstacles"]):
        x,y,hx,hy,height=(prop[k] for k in ("x","y","hx","hy","height"))
        if prop["kind"] == "tree":
            shape("Park_TreeTrunk_%02d" % idx, (x,y,height/2), (hx*2,hy*2,height), "Bark", "Cylinder")
            shape("Park_TreeCrown_%02d" % idx, (x,y,height+1.9), (5.8,5.8,4.5), "GrassLight" if idx%2 else "Sage", "Sphere")
        else:
            shape("Park_Rock_%02d" % idx, (x,y,height*.4), (hx*2,hy*2,height*1.5), prop["color"], "Sphere")
    for idx, block in enumerate(LAYOUT["buildings"]):
        x,y,hx,hy,height = (block[k] for k in ("x","y","hx","hy","height"))
        shape("Building_Block_%02d" % idx, (x,y,height/2-1.5), (hx*2,hy*2,height+3), block["color"])
        shape("Building_Roof_%02d" % idx, (x,y,height+.16), (hx*2+.3,hy*2+.3,.32), "Ink")
        # Window bands keep a rough city readable from every approach.
        for floor in range(3, int(height)-1, 4):
            for side in (-1,1):
                shape("Window_Front", (x,y+side*(hy+.025),floor), (hx*1.55,.04,1.2), "Window")
                shape("Window_Side", (x+side*(hx+.025),y,floor), (.04,hy*1.7,1.2), "Window")
    for x in axes:
        if x > split:
            continue
        for y in axes:
            for side in (-1,1):
                shape("Lamp_Pole", (x+11,y+side*12,2.8), (.12,.12,5.6), "Ink", "Cylinder")
                shape("Lamp_Head", (x+11,y+side*12,5.65), (.65,.65,.18), "GlowAmber")
    for side in (-1,1):
        shape("Boundary_EastWest", (0,side*208.5,-3), (418,1,10), "Navy")
        shape("Boundary_NorthSouth", (side*208.5,0,-3), (1,418,10), "Navy")
    gx,gy = LAYOUT["destination"]
    shape("Clinic_DeliveryZone", (gx,gy,.025), (14,14,.04), "GlowTeal", "Cylinder")
    shape("Clinic_Beacon", (gx+8,gy+8,10), (.5,.5,20), "GlowTeal", "Cylinder")
    shape("Clinic_Light", (gx+8,gy+8,20.4), (2,2,2), "GlowTeal", "Sphere")
    sign("Clinic_Sign", "CLINIC / DELIVERY", (195,186.9,9), height=.72)
    shape("Clinic_CrossA", (195,186.94,11.4), (3,.05,.65), "Coral")
    shape("Clinic_CrossB", (195,186.93,11.4), (.65,.05,3), "Coral")
    start = ACTORS.spawn_actor_from_class(unreal.PlayerStart, vector(*LAYOUT["start"],1+unreal.DeliveryTerrain.get_ground_height(*LAYOUT["start"])))
    start.set_actor_label("CourierStart")
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, vector(0, 0, 35), unreal.Rotator(pitch=-38,yaw=-25,roll=0))
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_editor_property("light_color", unreal.Color(255, 234, 207, 255))
    sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky = ACTORS.spawn_actor_from_class(unreal.SkyLight, vector(0, 0, 20))
    sky.light_component.set_editor_property("intensity", 1.0)
    sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    ACTORS.spawn_actor_from_class(unreal.SkyAtmosphere, vector(0, 0, 0))
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    fog = ACTORS.spawn_actor_from_class(unreal.ExponentialHeightFog, vector(0, 0, -5))
    fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_component.set_fog_density(.008)
    fog_component.set_fog_height_falloff(.15)
    viewport = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    viewport.set_level_viewport_camera_info(vector(LAYOUT["start"][0]-7, LAYOUT["start"][1]-1, 4), unreal.Rotator(pitch=-12,yaw=0,roll=0))
    if not LEVELS.save_current_level():
        raise RuntimeError("Could not save " + MAP)
    output_map = ROOT / "Content" / (MAP.removeprefix("/Game/") + ".umap")
    if not output_map.is_file():
        raise RuntimeError("Map save reported success, but .umap is missing")


def main():
    materials = {name: material(name, rgb) for name, rgb in PALETTE.items()}
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    for mat in materials.values():
        if not ASSETS.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("Could not save completed material shaders: "+mat.get_name())
    (ROOT/"Saved/material-usage-validation.json").write_text(json.dumps({
        mat.get_name():unreal.MaterialEditingLibrary.has_material_usage(mat,unreal.MaterialUsage.MATUSAGE_STATIC_MESH)
        for mat in materials.values()},indent=2)+"\n")
    audio()
    build_map(materials)
    MARKER.parent.mkdir(parents=True, exist_ok=True)
    MARKER.write_text(json.dumps({
        "schema": LAYOUT["schema"], "map": MAP, "palette": list(PALETTE),
        "layout_sha256": hashlib.sha256((ROOT / "Design/city_layout.json").read_bytes()).hexdigest(),
        "authoring": "Unreal Editor Python", "gameplay": "DeliveryRush C++",
        "engine_version": unreal.SystemLibrary.get_engine_version(),
    }, indent=2) + "\n")
    unreal.log("DELIVERY_RUSH_BOOTSTRAP_OK")


main()
