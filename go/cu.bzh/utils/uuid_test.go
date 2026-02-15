// uuid_test.go
package utils

import (
	"bytes"
	"testing"

	"github.com/google/uuid"
)

func TestUUIDV4Random(t *testing.T) {

	// Generate UUID v4
	uuid, err := uuid.NewRandom()
	if err != nil {
		t.Errorf("Failed to generate UUID v4 : %s", err.Error())
	}

	// Check if UUID is v4
	if uuid.Version() != 4 {
		t.Errorf("UUID is not v4 : %s", uuid.String())
	}

	// Check if UUID is valid
	uuidString := uuid.String()
	if len(uuidString) != 36 {
		t.Errorf("UUID is not 36 characters : %s", uuidString)
	}
}

func TestUUIDV4Parse(t *testing.T) {

	// Generate UUID v4
	uuidObj, err := uuid.NewRandom()
	if err != nil {
		t.Errorf("Failed to generate UUID v4 : %s", err.Error())
	}

	// Check if UUID is valid
	uuidString := uuidObj.String()
	if len(uuidString) != 36 {
		t.Errorf("UUID is not 36 characters : %s", uuidString)
	}

	// Parse UUID
	parsedUUID, err := uuid.Parse(uuidString)
	if err != nil {
		t.Errorf("Failed to parse UUID : %s", err.Error())
	}

	if uuidObj != parsedUUID {
		t.Errorf("Parsed UUID is not equal to generated UUID")
	}

	// ---
	uuidBytes := uuidObj[:]
	uuidBytes2, err := uuidObj.MarshalBinary()
	if err != nil {
		t.Errorf("Failed to marshal binary : %s", err.Error())
	}
	if !bytes.Equal(uuidBytes, uuidBytes2) {
		t.Errorf("UUID bytes are not equal")
	}
}
