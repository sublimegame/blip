package main

import (
	"bytes"
	"errors"
	"math"

	gltf "github.com/qmuntal/gltf"
)

// decodes .glb data and returns a gltf.Document
func decodeGltf(data []byte) (*gltf.Document, error) {
	dec := gltf.NewDecoder(bytes.NewReader(data))
	doc := &gltf.Document{}
	err := dec.Decode(doc)
	if err != nil {
		return nil, err
	}
	return doc, nil
}

// gltfParentRootNodesIfNeeded parents root nodes if there are more than one.
// It returns an error if there are no root nodes.
func gltfParentRootNodesIfNeeded(doc *gltf.Document) error {
	if doc == nil || doc.Scenes == nil || len(doc.Scenes) == 0 {
		return errors.New("no scenes in gltf document")
	}
	// Use the default scene if set, otherwise the first scene
	sceneIdx := 0
	if doc.Scene != nil {
		sceneIdx = *doc.Scene
	}
	scene := doc.Scenes[sceneIdx]
	if len(scene.Nodes) == 0 {
		return errors.New("no root nodes in scene")
	}
	if len(scene.Nodes) == 1 {
		return nil // nothing to do
	}
	// Create a new parent node
	parent := &gltf.Node{
		Name:     "Root",
		Children: make([]int, len(scene.Nodes)),
	}
	for i, idx := range scene.Nodes {
		parent.Children[i] = idx
	}
	doc.Nodes = append(doc.Nodes, parent)
	parentIdx := len(doc.Nodes) - 1
	scene.Nodes = []int{parentIdx}
	return nil
}

// gltfApplyRotation applies the rotation to the root node.
// It doesn't do anything if the given rotation to apply is [0,0,0].
// It returns an error if there's more than one root node when called.
func gltfApplyRotation(doc *gltf.Document, rotation []float64) error {
	if doc == nil || doc.Scenes == nil || len(doc.Scenes) == 0 {
		return errors.New("no scenes in gltf document")
	}
	sceneIdx := 0
	if doc.Scene != nil {
		sceneIdx = *doc.Scene
	}
	scene := doc.Scenes[sceneIdx]
	if len(scene.Nodes) == 0 {
		return errors.New("no root nodes in scene")
	}
	if len(scene.Nodes) > 1 {
		return errors.New("more than one root node")
	}
	if len(rotation) != 3 {
		return errors.New("rotation must be a 3-element array [x, y, z]")
	}
	if rotation[0] == 0 && rotation[1] == 0 && rotation[2] == 0 {
		return nil // nothing to do
	}
	rootIdx := scene.Nodes[0]
	node := doc.Nodes[rootIdx]
	// Convert Euler angles (XYZ, radians) to quaternion
	qx, qy, qz, qw := eulerToQuaternion(rotation[0], rotation[1], rotation[2])
	node.Rotation = [4]float64{qx, qy, qz, qw}
	return nil
}

// gltfApplyScale applies the scale to the root node.
// It doesn't do anything if the given scale is 1.0.
// It returns an error if there's more than one root node when called.
func gltfApplyScale(doc *gltf.Document, scale float64) error {
	if doc == nil || doc.Scenes == nil || len(doc.Scenes) == 0 {
		return errors.New("no scenes in gltf document")
	}
	sceneIdx := 0
	if doc.Scene != nil {
		sceneIdx = *doc.Scene
	}
	scene := doc.Scenes[sceneIdx]
	if len(scene.Nodes) == 0 {
		return errors.New("no root nodes in scene")
	}
	if len(scene.Nodes) > 1 {
		return errors.New("more than one root node")
	}
	if scale == 1.0 {
		return nil // nothing to do
	}
	rootIdx := scene.Nodes[0]
	node := doc.Nodes[rootIdx]
	node.Scale = [3]float64{scale, scale, scale}
	return nil
}

// gltfParentRootNodeIfNeeded parents the root node if it has a non-zero rotation or non-1.0 scale.
// It returns an error if there's no root node.
// It returns an error if there are more than one root node when called.
// It doesn't do anything if the root node has no rotation and a scale of 1.0.
func gltfParentRootNodeIfNeeded(doc *gltf.Document) error {
	if doc == nil || doc.Scenes == nil || len(doc.Scenes) == 0 {
		return errors.New("no scenes in gltf document")
	}
	sceneIdx := 0
	if doc.Scene != nil {
		sceneIdx = *doc.Scene
	}
	scene := doc.Scenes[sceneIdx]
	if len(scene.Nodes) == 0 {
		return errors.New("no root nodes in scene")
	}
	if len(scene.Nodes) > 1 {
		return errors.New("more than one root node")
	}
	rootIdx := scene.Nodes[0]
	node := doc.Nodes[rootIdx]
	rot := node.Rotation
	scale := node.Scale
	if (rot[0] == 0 && rot[1] == 0 && rot[2] == 0 && rot[3] == 0) &&
		(scale[0] == 1.0 && scale[1] == 1.0 && scale[2] == 1.0) {
		return nil // nothing to do
	}
	// Create a new parent node with identity transform
	parent := &gltf.Node{
		Name:     "Root",
		Children: []int{rootIdx},
	}
	doc.Nodes = append(doc.Nodes, parent)
	parentIdx := len(doc.Nodes) - 1
	scene.Nodes = []int{parentIdx}
	return nil
}

// saves document in .glb format
func encodeGltf(doc *gltf.Document) ([]byte, error) {
	buf := &bytes.Buffer{}
	enc := gltf.NewEncoder(buf)
	enc.AsBinary = true
	err := enc.Encode(doc)
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

// Helper: convert Euler angles (XYZ, radians) to quaternion (x, y, z, w)
func eulerToQuaternion(x, y, z float64) (qx, qy, qz, qw float64) {
	// Reference: https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	cx := cos(x / 2)
	sx := sin(x / 2)
	cy := cos(y / 2)
	sy := sin(y / 2)
	cz := cos(z / 2)
	sz := sin(z / 2)
	qw = cx*cy*cz + sx*sy*sz
	qx = sx*cy*cz - cx*sy*sz
	qy = cx*sy*cz + sx*cy*sz
	qz = cx*cy*sz - sx*sy*cz
	return
}

// Helper: math functions
func sin(x float64) float64 {
	return math.Sin(x)
}
func cos(x float64) float64 {
	return math.Cos(x)
}
