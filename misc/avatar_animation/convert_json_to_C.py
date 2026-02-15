
import json
import sys

print("Converting avatar animation : JSON to C ...")
print("===========================================")

# internal representation of the animation
animation = {
    'name' : '',
    'speed' : 0.0,
    'parts' : {},
}

# generate C code for animation "walk"
def generateCCodeForWalkAnimation(animation):
    animationName = animation['name']
    partsToProcess = ['lleg', 'lfoot', 'rleg', 'rfoot', 'larm', 'lhand', 'rarm', 'rhand', 'head', 'body']
    
    print('---------- C Code ---------- (', animationName, ')')
    print('')
    print('p->' + animationName + ' = animation_new();')
    
    parts = animation['parts']
    for partName in partsToProcess:

        # hack : disable legs and feet
        # if partName == 'lleg' or partName == 'rleg' or partName == 'lfoot' or partName == 'rfoot':
        #     continue

        part = parts[partName]
        varName = animationName + '_' + partName + 'K'
        print('') # empty ligne for readability
        print('Keyframes *' + varName + ' = keyframes_new(NULL, "' + partName + '");')
        frames = part['frames']
        framesCount = len(frames)
        for i, frame in enumerate(frames):
            weight = float(i) / float(framesCount)
            printFrame(frame, partName, weight, varName)

        # add last frame, identical to first frame
        weight = 1.0
        printFrame(frames[0], partName, weight, varName)

        print('animation_add(p->' + animationName + ', ' + varName + ');')

# 
def convertPartNameFromJsonToC(jsonName):
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
    # returns None when the key is not found
    return knownParts.get(jsonName)

# 
def appendBodyPartFramePosition(bodyPartCName, frameIdx, posX, posY, posZ):
    # print('append:', bodyPartCName, '   ', frameIdx, '  ', posX, posY, posZ)
    parts = animation['parts']

    if parts.get(bodyPartCName) is None:
        parts[bodyPartCName] = {}
    part = parts[bodyPartCName]
    
    if part.get('frames') is None:
        part['frames'] = []
    frames = part['frames']

    #  frameIdx is just over the limit of the array
    if frameIdx == len(frames):
        frames.append({ 'position' : { 'x' : posX, 'y' : posY, 'z' : posZ } })
    else:
        frames[frameIdx]['position'] = { 'x' : posX, 'y' : posY, 'z' : posZ }

# 
def appendBodyPartFrameRotation(bodyPartCName, frameIdx, rotX, rotY, rotZ):
    # print('append:', bodyPartCName, '   ', frameIdx, '  ', rotX, rotY, rotZ)
    parts = animation['parts']

    if parts.get(bodyPartCName) is None:
        parts[bodyPartCName] = {}
    part = parts[bodyPartCName]
    
    if part.get('frames') is None:
        part['frames'] = []
    frames = part['frames']

    #  frameIdx is just over the limit of the array
    if frameIdx == len(frames):
        frames.append({ 'rotation' : { 'x' : rotX, 'y' : rotY, 'z' : rotZ } })
    else:
        frames[frameIdx]['rotation'] = { 'x' : rotX, 'y' : rotY, 'z' : rotZ }

# 
def printFrame(frame, partName, weight, varName):
    
    # position
    position = frame['position'] # position.x/y/z
    posX = position['x']
    posY = position['y']
    posZ = position['z']

    # rotation
    rotation = frame['rotation'] # rotation.x/y/z
    rotX = rotation['x']
    rotY = rotation['y']
    rotZ = rotation['z']
    
    # hack (position)
    partHasPosition = False
    if partName == 'body': # positions are only allowed on body for now
        partHasPosition = True
        posX = (posX - 0.0) * 2.0
        posY = (posY - 7.0) * 2.0
        posZ = (posZ - 0.0) * 2.0
    elif partName == 'rarm':
        partHasPosition = True
        posX = (posX * -1.0)
        posY = (posY * 1.0)
        posZ = (posZ * -1.0)
    else:
        posX = 0.0
        posY = 0.0
        posZ = 0.0

    # hack (rotation)
    if (partName == 'larm' or 
        partName == 'rarm' or
        partName == 'lleg' or 
        partName == 'rleg' or
        partName == 'lfoot' or 
        partName == 'rfoot' or 
        partName == 'lhand' or 
        partName == 'rhand'):
        rotX *= -1.0
        rotZ *= -1.0
    elif partName == 'body':
        rotX = 0
        rotY = 0
        rotZ = 0
    # elif partName == 'head':

    if partHasPosition == True:
        print('float3_set(&pos, ' + str(posX) + ', ' + str(posY) + ', ' + str(posZ) + ');')
        print('float3_set(&rot, ' + str(rotX) + ', ' + str(rotY) + ', ' + str(rotZ) + ');')
        print('keyframes_add_new(' + varName + ', ' + str(weight) + ', &pos, &rot, NULL, NULL);')
    else:
        print('float3_set(&rot, ' + str(rotX) + ', ' + str(rotY) + ', ' + str(rotZ) + ');')
        print('keyframes_add_new(' + varName + ', ' + str(weight) + ', NULL, &rot, NULL, NULL);')


args = sys.argv
if len(args) != 3:
    print('❌ wrong number of arguments')
    exit(1)

animation['name'] = args[1]
filename = args[2]

print('animation name:', animation['name'])
print('JSON file name:', filename)

# Opening JSON file
f = open(filename)
  
# returns JSON object as a dictionary
data = json.load(f)

# --- Parsing ---

# Animation speed
animation['speed'] = data.get('speed', 1.0) # second argument is the default value

# Animation frames
frames = data['frames']

# Iterating over the frames
for frameIdx, frame in enumerate(frames):

    # ----- positions -----
    positions = frame['positions']
    for bodyPartJsonName in positions:
        positionValues = positions[bodyPartJsonName]

        # convert body part name from JSON to C version
        bodyPartCName = convertPartNameFromJsonToC(bodyPartJsonName)
        if bodyPartCName is None:
            print('❌ ERROR: unsupported json body part', bodyPartJsonName)
        
        # get rotation values
        posX = positionValues[0]
        posY = positionValues[1]
        posZ = positionValues[2]

        # store rotation value into the internal representation
        appendBodyPartFramePosition(bodyPartCName, frameIdx, posX, posY, posZ)

    # ----- rotations -----
    rotations = frame['rotations']
    for bodyPartJsonName in rotations:
        rotationValues = rotations[bodyPartJsonName]

        # convert body part name from JSON to C version
        bodyPartCName = convertPartNameFromJsonToC(bodyPartJsonName)
        if bodyPartCName is None:
            print('❌ ERROR: unsupported json body part', bodyPartJsonName)
        
        # get rotation values
        rotX = rotationValues[0]
        rotY = rotationValues[1]
        rotZ = rotationValues[2]

        # store rotation value into the internal representation
        appendBodyPartFrameRotation(bodyPartCName, frameIdx, rotX, rotY, rotZ)

# output C code
generateCCodeForWalkAnimation(animation)

# --- Closing file ---

f.close()