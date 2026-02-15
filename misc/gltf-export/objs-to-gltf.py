# Combines .obj files found in directory into one (.gltf or .fbx) + .png texture
# Sets center of the whole group to {0.5, 0.0, 0.5}

import glob
import os
import sys
import json
from itertools import chain

import bpy
from mathutils import Euler, Matrix, Vector

# TO RUN:
# /path/to/blender -b -P assemble_avatar.py -- input_folder output_file

def help():
    h = '--------------\n'
    h += 'OBJs to GLTF + PNG\n'
    h += 'usage: <blender script> input_folder output_file\n\n'
    h += 'input_folder: path to input folder containg OBJ files\n'
    h += 'output_file:  path to output GLTF or FBX\n'
    print(h)
    sys.exit(0)

# removes all objects from the scene
def clear_scene():
    bpy.ops.object.select_all(action='DESELECT')
    for obj in bpy.data.objects:
        obj.select_set(True)
        bpy.ops.object.delete(use_global=False)

# imports all .obj found in given folder
# no object selected when the function returns
def import_objs(folder_path):

    metadata = None
    f = None

    metadataPath = os.path.join(folder_path, 'metadata.json')

    try:
        f = open(metadataPath, 'rb')
    except Exception as e:
        f = None
        print("can't open metadata.json")

    if f != None:
        try:
            metadata = json.load(f)
        except Exception as e:
            metadata = None
            print("❌ metadata.json", e)

    obj_path = os.path.join(folder_path, '*.obj')
    obj_files = glob.glob(obj_path)

    objects = []
    parts = {}
    texture = None

    for filepath in obj_files:
        bpy.ops.import_scene.obj(filepath=filepath)
        obj = bpy.context.selected_objects[0]

        if metadata != None and metadata['parts'] != None:
            for part in metadata['parts']:
                if part in os.path.basename(filepath):
                    obj.name = part
                    if part in parts:
                        print("❌ importing part more than once:", part)
                    else:
                        parts[part] = obj
                    break

        material = obj.material_slots[0].material
        texture = material.node_tree.nodes['Image Texture'].image
        # obj.rotation_mode = 'YXZ'
        objects.append(obj)
        bpy.ops.object.select_all(action='DESELECT')

    return objects, texture, parts, metadata

# loads keyframes from avatar-animation.json if found
def mergeAvatarAnimation(metadata, folder_path):
    if metadata == None or input_folder == None:
        return

    path = os.path.join(folder_path, 'animations.json')
    # print("avatar animation path:", path)

    try:
        f = open(path, 'rb')
    except Exception:
        print("NO ANIMATION FILE")
        return

    try:
        animation = json.load(f)
    except Exception:
        return

    def mergePositionFrame(part, positionFrames, i):

        # knownParts = {
        #     'pelvis': 'body'
        # }
        knownParts = {
            'lLeg': 'lleg',
            'lFoot': 'lfoot',
            'rLeg': 'rleg',
            'rFoot': 'rfoot',
            'lArm': 'larm',
            'lForeArm': 'lhand',
            'rArm': 'rarm',
            'rForeArm': 'rhand',
            'head': 'head',
            'pelvis': 'body'
        }

        if (part in knownParts) == False:
            return

        corresponding = knownParts[part]

        try:
            c = metadata['parts'][corresponding]
        except Exception:
            return

        if ('frames' in c) == False:
            c['frames'] = []

        frames = c['frames']

        if i >= len(frames):
            frames.append({})

        # print("----- mergePositionFrame")

        f = frames[i]
        p = frame['positions'][part]
        f['position'] = p
        # print(f['position'])

    def mergeRotationFrame(part, rotationFrames, i):

        knownParts = {
            'lLeg': 'lleg',
            'lFoot': 'lfoot',
            'rLeg': 'rleg',
            'rFoot': 'rfoot',
            'lArm': 'larm',
            'lForeArm': 'lhand',
            'rArm': 'rarm',
            'rForeArm': 'rhand',
            'head': 'head',
            'pelvis': 'body'
        }

        if (part in knownParts) == False:
            return

        corresponding = knownParts[part]

        try:
            c = metadata['parts'][corresponding]
        except Exception:
            return

        if ('frames' in c) == False:
            c['frames'] = []

        frames = c['frames']

        if i >= len(frames):
            frames.append({})

        f = frames[i]
        # f['rotation'] = rotationFrames[part]
        r = frame['rotations'][part]
        # converted_r = [r[2], -r[1], r[0]]
        
        # -r[1] -> Y
        # r[0] -> X
        # r[2] -> Z
        # ZYX

        # converted_r = [r[2], -r[1], r[0]] #ZYX
        converted_r = [-r[0], -r[1], r[2]]

        f['rotation'] = converted_r

    i = 0
    for frame in  animation['frames']:
        for part in frame['rotations']:
            mergeRotationFrame(part, frame['rotations'], i)
            mergePositionFrame(part, frame['positions'], i)

        i = i + 1

def copyFirstFrame(metadata):
    if metadata == None:
        return

    for part in metadata['parts']:
        try:
            frames = metadata['parts'][part]['frames']
        except Exception as e:
            continue

        metadata['parts'][part]['frames'].append(frames[0])


def setScales(parts, metadata):
    if metadata == None or parts == None:
        return

    for part in metadata['parts']:
        if (part in parts) == False:
            continue

        obj = parts[part]

        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)

        try:
            scale = metadata['parts'][part]['scale']
        except Exception:
            continue

        if type(scale) == float:
            scale = [scale, scale, scale]

        obj.scale = scale


def setPivots(parts, metadata):
    if metadata == None or parts == None:
        return

    for part in metadata['parts']:
        if (part in parts) == False:
            continue
        
        obj = parts[part]

        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)

        try:
            pivot = metadata['parts'][part]['pivot']

        except Exception:
            bpy.ops.object.origin_set(type='ORIGIN_CENTER_OF_VOLUME', center='MEDIAN')
            continue

        minimum, maximum = getBoundaries(obj)

        cursor_location = bpy.context.object.matrix_world @ Vector((minimum[0] + (maximum[0] - minimum[0]) * pivot[0],
                            minimum[1] + (maximum[1] - minimum[1]) * pivot[1],
                            minimum[2] + (maximum[2] - minimum[2]) * pivot[2]))

        bpy.context.scene.cursor.location = cursor_location
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR', center='MEDIAN')
        

def setHierarchy(parts, metadata):
    if metadata == None or parts == None:
        return

    for part in metadata['parts']:
        if (part in parts) == False:
            continue

        obj = parts[part]

        try:
            parentName = metadata['parts'][part]['parent']
        except Exception as e:
            continue

        parent = parts[parentName]
        if parent == None:
            continue

        obj.parent = parent
        obj.matrix_parent_inverse = parent.matrix_world.inverted()


def setKeyframes(parts, metadata):

    if metadata == None or parts == None:
        return

    try:
        frameFactor = metadata['frameFactor']
    except:
        frameFactor = 3

    for part in metadata['parts']:

        frames = None

        if (part in parts) == False:
            continue

        obj = parts[part]

        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)

        try:
            frames = metadata['parts'][part]['frames']
        except Exception as e:
            continue

        i = 0
        for frame in frames:
            try:
                hide = frame['hide']
                print("hide:", hide)
                
                if hide:
                    obj.scale = [0,0,0]
                else:
                    obj.scale = [1,1,1]
                
                obj.keyframe_insert(data_path="scale", frame=(i*frameFactor))

            except Exception:
                pass

            try:
                scale = frame['scale']
                print("scale:", scale)
                if type(scale) == float:
                    scale = [scale, scale, scale]

                obj.scale = scale
                obj.keyframe_insert(data_path="scale", frame=(i*frameFactor))
            except Exception:
                pass

            # if part == "body":
            #     try:
            #         position = frame['position']

            #         save = obj.location.z
            #         # print("----- DELTA:", position[1] - 7)
            #         obj.location.z = obj.location.z + (position[1]) * 0.15

            #         obj.keyframe_insert(data_path="location", frame=(i*3))
            #         # sobj.location.z = save
            #     except Exception as e:
            #         print("⚠️: ", e)
            #         pass

            if part == "rleg" or part == "lleg" or part == "body" or part == "larm" or part == "rarm":
                try:

                    origin = [0, 100, 0]

                    if part == "rleg":
                        origin = [2.5, 0, 0]
                    elif part == "lleg":
                        origin = [-2.5, 0, 0]
                    elif part == "larm":
                        origin = [-5, 7, 0]
                    elif part == "rarm":
                        origin = [5, 7, 0]
                    elif part == "body":
                        # origin = [0, 6.5, 0]
                        origin = [0, 0, 0]

                    origin[0] = origin[0] * 0.2
                    origin[1] = origin[1] * 0.2
                    origin[2] = origin[2] * 0.2

                    position = [frame['position'][0], frame['position'][1], frame['position'][2]] # hard copy
                    position[0] = position[0] * 0.2
                    position[1] = position[1] * 0.2
                    position[2] = position[2] * 0.2

                    offset = [0,0,0]
                    offset[0] = origin[0] - position[0]
                    offset[1] = origin[1] - position[1]
                    offset[2] = origin[2] - position[2]

                    save = [obj.location.x, obj.location.y, obj.location.z]

                    print("--", part, "--")
                    print("    origin:", origin)
                    print("    position:", position)
                    print("    offset:", offset)
                    print("    translation:", obj.matrix_local.translation)
 
                    # HACK to make it work quickly
                    # There's an issue somewhere with transformation / local/global space conversion?
                    if part == "body":
                        obj.matrix_local.translation = Vector((offset[0], offset[2], -offset[1] * 1.5)) + obj.matrix_local.to_translation()
                    elif part == "larm" or part == "rarm":
                        obj.matrix_local.translation = Vector((-offset[0], offset[1], offset[2])) + obj.matrix_local.to_translation()
                    else:
                        obj.matrix_local.translation = Vector((offset[0], offset[1], offset[2])) + obj.matrix_local.to_translation()

                    obj.keyframe_insert(data_path="location", frame=(i*3))
                    obj.location.x = save[0]
                    obj.location.y = save[1]
                    obj.location.z = save[2]

                except Exception as e:
                    print("⚠️: ", e)
                    pass

            try:
                rotation = frame['rotation']
                mat = obj.matrix_local
                # rot = Euler((rotation[0], rotation[1], rotation[2]), 'YXZ')
                # rot = Euler((rotation[0], rotation[1], rotation[2]), 'YZX')
                # rot = Euler((rotation[0], rotation[1], rotation[2]), 'ZYX')
                rot = Euler((rotation[0], rotation[1], rotation[2]), 'ZXY')
                # rot = Euler((rotation[0], rotation[1], rotation[2]), 'XYZ')
                # rot = Euler((rotation[0], rotation[1], rotation[2]), 'XZY')
                obj.matrix_local = mat @ rot.to_matrix().to_4x4()
                obj.keyframe_insert(data_path="rotation_euler", frame=(i*3))

            except Exception as e:
                pass
            

            i = i + 1

def rename(objects):

    for obj in objects:
        print("NAME:", obj.name)
        if obj.name == "top":
            obj.name = "outfit"

# exports all objets in parameter to filepath (.gltf or .fbx)
# /!\ assuming all objects have the same texture,
# exporting texture found in first object.
def export(objects, texture, output_file):

    bpy.ops.object.select_all(action='DESELECT')

    for obj in objects:
        obj.select_set(True)

    if output_file.endswith('.gltf'):
        bpy.ops.export_scene.gltf(
            filepath=output_file,
            use_selection=False,
            export_format='GLTF_EMBEDDED')
    elif output_file.endswith('.fbx'):
        bpy.ops.export_scene.fbx(
            filepath=output_file,
            bake_anim_use_all_bones=False,
            bake_anim_use_nla_strips=False,
            bake_anim_use_all_actions=False,
            bake_anim_force_startend_keying=False,
            use_selection=True,
            apply_unit_scale=True,
            apply_scale_options='FBX_SCALE_UNITS',
            axis_forward='Y',
            axis_up='Z')
    else:
        print('Can only export to GLTF or FBX format.')
        return

    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    # recalculate outside normals
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')

    path_without_extension, _ = os.path.splitext(output_file)
    texture.save_render(filepath=path_without_extension + '.png')


def getBoundaries(obj):

    minimum = None
    maximum = None
    first = True

    vertices = obj.data.vertices
    for vertex in vertices:
        if first:
            minimum = [vertex.co.x, vertex.co.y, vertex.co.z]
            maximum = [vertex.co.x, vertex.co.y, vertex.co.z]
            first = False
        else:
            if vertex.co.x < minimum[0]: minimum[0] = vertex.co.x
            if vertex.co.y < minimum[1]: minimum[1] = vertex.co.y
            if vertex.co.z < minimum[2]: minimum[2] = vertex.co.z

            if vertex.co.x > maximum[0]: maximum[0] = vertex.co.x
            if vertex.co.y > maximum[1]: maximum[1] = vertex.co.y
            if vertex.co.z > maximum[2]: maximum[2] = vertex.co.z

    return minimum, maximum


def center_objects(objects):
    # bpy.ops.object.select_all(action='DESELECT')
    # for obj in objects:
    #     obj.select_set(True)

    minimum = None
    maximum = None
    first = True

    for obj in objects:
        vertices = obj.data.vertices
        for vertex in vertices:
            if first:
                minimum = [vertex.co.x, vertex.co.y, vertex.co.z]
                maximum = [vertex.co.x, vertex.co.y, vertex.co.z]
                first = False
            else:
                if vertex.co.x < minimum[0]: minimum[0] = vertex.co.x
                if vertex.co.y < minimum[1]: minimum[1] = vertex.co.y
                if vertex.co.z < minimum[2]: minimum[2] = vertex.co.z

                if vertex.co.x > maximum[0]: maximum[0] = vertex.co.x
                if vertex.co.y > maximum[1]: maximum[1] = vertex.co.y
                if vertex.co.z > maximum[2]: maximum[2] = vertex.co.z


    pivot = [0.5, 0, 0.5]

    delta = [minimum[0] + (maximum[0] - minimum[0]) * pivot[0],
            minimum[1] + (maximum[1] - minimum[1]) * pivot[1],
            minimum[2] + (maximum[2] - minimum[2]) * pivot[2]]

    for obj in objects:
        vertices = obj.data.vertices
        for vertex in vertices:
            vertex.co.x -= delta[0]
            vertex.co.y -= delta[1]
            vertex.co.z -= delta[2]

def run(input_folder, output_file):
    clear_scene()

    # objects: array of all imported objects
    # parts: objects indexed by recognized part name
    objects, texture, parts, metadata = import_objs(input_folder)

    # print("METADATA:", metadata)
    # print("PARTS:", parts)

    mergeAvatarAnimation(metadata, input_folder)
    copyFirstFrame(metadata)

    center_objects(objects)

    setPivots(parts, metadata)
    setScales(parts, metadata)
    setHierarchy(parts, metadata)
    setKeyframes(parts, metadata)

    rename(objects)

    export(objects, texture, output_file)

if __name__ == '__main__':
    argv = sys.argv
    argv = argv[argv.index('--') + 1:]
    
    if len(argv) < 1:
        help()
        
    input_folder = argv[0]
    output_file = argv[1]
    if not output_file.endswith('.gltf') and not output_file.endswith('.fbx'):
        print('Can only export to GLTF or FBX format.')
        help()
    run(input_folder, output_file)
