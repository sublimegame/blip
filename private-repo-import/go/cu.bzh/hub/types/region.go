package types

import (
	"fmt"
	"strings"
)

// -----------------------------
// Exposed
// -----------------------------

// Getter functions to access the predefined regions
func RegionZero() Region { return regionZero }
func RegionEU() Region   { return regionEU }
func RegionSG() Region   { return regionSG }
func RegionUS() Region   { return regionUS }

// Region is the public interface for region type
type Region interface {
	String() string
}

// NewRegion creates a new Region from a string, returns nil if invalid
func NewRegion(name string) Region {
	// ensure the name is lowercase
	if lowered := strings.ToLower(name); name != lowered {
		fmt.Printf("⚠️ NewRegion: name is not lowercase: %s\n", name)
		name = lowered
	}
	// returns nil if name is not found in the map
	return regionNameToRegion[name]
}

// String returns the string representation of the Region
func (r region) String() string {
	return string(r)
}

// -----------------------------
// Not exposed
// -----------------------------

const (
	regionNameEU        = "eu"
	regionNameSG        = "sg"
	regionNameUS        = "us"
	regionNameZero      = ""
	regionNameUS_Legacy = "usa" // last version using this : 0.1.12
)

var (
	regionEU   Region = newRegionPrivate(regionNameEU) // Europe
	regionSG   Region = newRegionPrivate(regionNameSG) // Singapore
	regionUS   Region = newRegionPrivate(regionNameUS) // United States
	regionZero Region = newRegionPrivate(regionNameZero)

	regionNameToRegion = map[string]Region{
		regionNameEU:        regionEU,
		regionNameSG:        regionSG,
		regionNameUS:        regionUS,
		regionNameZero:      regionZero,
		regionNameUS_Legacy: regionUS,
	}

	// validRegionNames = []string{regionNameEU, regionNameSG, regionNameUS}
)

// region represents a game region identifier
type region string

// Creates a new region from a string, without any validation
func newRegionPrivate(name string) Region {
	r := region(name)
	return &r
}
