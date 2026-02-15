package main

import (
	"testing"
)

// TestCheckPhoneNumber tests the CheckPhoneNumber function
func TestCheckPhoneNumber(t *testing.T) {
	tests := []struct {
		input           string
		outputIsValid   bool
		outputFormatted string
	}{
		{"Hello+123-456!789", false, ""},
		{"NoNumbersHere!", false, ""},
		{"+OnlyPlus", false, ""},
		{"A1B2C3", false, ""},
		{"16502356131", true, "+16502356131"},
		{"+16502356131", true, "+16502356131"},
		{"33767417925", true, "+33767417925"},
		{"+33767417925", true, "+33767417925"},
	}

	for _, test := range tests {
		isValid, formatted, err := CheckPhoneNumber(test.input)
		if err != nil {
			t.Error("error:", err.Error())
		}
		if isValid != test.outputIsValid {
			t.Errorf("For input '%s', expected isValid to be '%t', but got '%t'", test.input, test.outputIsValid, isValid)
		}
		if formatted != test.outputFormatted {
			t.Errorf("For input '%s', expected formatted to be '%s', but got '%s'", test.input, test.outputFormatted, formatted)
		}
	}
}
