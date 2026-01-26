package types

import (
	"testing"
)

func TestNewRegion(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    Region
		wantNil bool
	}{
		{name: "valid eu region", input: "eu", want: RegionEU()},
		{name: "valid sg region", input: "sg", want: RegionSG()},
		{name: "valid us region", input: "us", want: RegionUS()},
		{name: "uppercase EU", input: "EU", want: RegionEU()},
		{name: "mixed case SG", input: "Sg", want: RegionSG()},
		{name: "empty string", input: "", want: RegionZero()},
		{name: "invalid region", input: "invalid", wantNil: true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := NewRegion(tt.input)
			if tt.wantNil {
				if got != nil {
					t.Errorf("NewRegion(%q) = %v, want nil", tt.input, got)
				}
				return
			}
			if got != tt.want {
				t.Errorf("NewRegion(%q) = %v, want %v", tt.input, got, tt.want)
			}
		})
	}
}

func TestRegion_String(t *testing.T) {
	tests := []struct {
		name   string
		region Region
		want   string
	}{
		{
			name:   "EU region",
			region: RegionEU(),
			want:   regionNameEU,
		},
		{
			name:   "SG region",
			region: RegionSG(),
			want:   regionNameSG,
		},
		{
			name:   "US region",
			region: RegionUS(),
			want:   regionNameUS,
		},
		{
			name:   "nil region",
			region: RegionZero(),
			want:   regionNameZero,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := tt.region.String(); got != tt.want {
				t.Errorf("Region.String() = %v, want %v", got, tt.want)
			}
		})
	}
}
