package gameserver

import "errors"

// --------------------------------------------------
// Constants
// --------------------------------------------------

const (
	ModeBoth Mode = "both" // [@gdevillele] I don't think this is used.
	ModeDev  Mode = "dev"
	ModePlay Mode = "play"
)

var (
	ErrInvalidMode error = errors.New("invalid gameserver mode")
)

// --------------------------------------------------
// Types
// --------------------------------------------------

// The mode in which a gameserver is running.
// It defines some aspect of the gameserver's behavior.
// (whether the script publishing is allowed, for instance)
type Mode string

// --------------------------------------------------
// Exposed functions
// --------------------------------------------------

// ValidateMode checks whether the provided mode is valid.
func ValidateMode(mode Mode) error {
	switch mode {
	case ModeBoth, ModeDev, ModePlay:
		return nil // success
	}
	return ErrInvalidMode // error
}
