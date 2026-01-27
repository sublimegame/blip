package main

import (
	"flag"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"os"
	"path/filepath"

	"github.com/blackironj/panorama/conv"
)

func main() {
	flag.Parse()

	if len(flag.Args()) < 1 || len(flag.Args()) > 2 {
		fmt.Println("Usage: cubemap <input.png> [output.ext]")
		fmt.Println("  If output file is not provided, will use input filename with -cubemap suffix")
		fmt.Println("  Supported output formats: .png, .ktx")
		os.Exit(1)
	}

	inputPath := flag.Arg(0)
	ext := filepath.Ext(inputPath)
	if ext != ".png" {
		fmt.Println("Error: Input file must be a PNG image")
		os.Exit(1)
	}

	// Read input image
	inImage, _, err := conv.ReadImage(inputPath)
	if err != nil {
		fmt.Printf("Error reading input image: %v\n", err)
		os.Exit(1)
	}

	// Check if image is 2:1 ratio (equirectangular)
	bounds := inImage.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()
	if width != height*2 {
		fmt.Printf("Error: Image must be in equirectangular projection (2:1 ratio). Current dimensions: %dx%d\n", width, height)
		os.Exit(1)
	}

	// Determine output path
	var outPath string
	if len(flag.Args()) == 2 {
		outPath = flag.Arg(1)
		outExt := filepath.Ext(outPath)
		if outExt != ".png" && outExt != ".ktx" {
			fmt.Println("Error: Output file must have .png or .ktx extension")
			os.Exit(1)
		}
	} else {
		outPath = inputPath[:len(inputPath)-len(ext)] + "-cubemap" + ext
	}

	edgeLen := 512
	sides := []string{"front", "back", "left", "right", "top", "bottom"}
	canvases := conv.ConvertEquirectangularToCubeMap(edgeLen, inImage, sides)

	if err := conv.WriteImage(canvases, ".", "png", sides, 1); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}

	// Combine images into a single image
	images := map[string]struct{ x, y int }{
		"top.png":    {1, 0},
		"left.png":   {0, 1},
		"back.png":   {3, 1},
		"right.png":  {2, 1},
		"front.png":  {1, 1},
		"bottom.png": {1, 2},
	}

	// Create a new RGBA image that's 4x3 tiles of edgeLen
	combined := image.NewRGBA(image.Rect(0, 0, edgeLen*4, edgeLen*3))

	// Load and copy each image into position
	for filename, pos := range images {
		file, err := os.Open(filename)
		if err != nil {
			fmt.Printf("Error opening face image: %v\n", err)
			os.Exit(1)
		}

		img, _, err := image.Decode(file)
		if err != nil {
			file.Close()
			fmt.Printf("Error decoding face image: %v\n", err)
			os.Exit(1)
		}

		x := pos.x * edgeLen
		y := pos.y * edgeLen

		draw.Draw(combined, image.Rect(x, y, x+edgeLen, y+edgeLen), img, image.Point{0, 0}, draw.Over)

		file.Close()
		// os.Remove(filename)
	}

	// Handle output format
	outExt := filepath.Ext(outPath)
	if outExt == ".ktx" {
		fmt.Println("KTX export is a work in progress")
		os.Exit(0)
	}

	outFile, err := os.Create(outPath)
	if err != nil {
		fmt.Printf("Error creating output file: %v\n", err)
		os.Exit(1)
	}
	defer outFile.Close()

	if err := png.Encode(outFile, combined); err != nil {
		fmt.Printf("Error encoding output image: %v\n", err)
		os.Exit(1)
	}

	fmt.Printf("Successfully created cubemap at: %s\n", outPath)
}
