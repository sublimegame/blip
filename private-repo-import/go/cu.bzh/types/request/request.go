package request

// VerifySession ...
type VerifySession struct {
	Hash string `json:"hash"`
}

// Login ...
type Login struct {
	UsernameOrEmail string `json:"usernameOrEmail"`
	Password        string `json:"password"`
}

// IsLoggedIn ...
type IsLoggedIn struct{}

// Game (update, create)
type Game struct {
	ID           string `json:"id,omitempty"` // ID is empty when creating a new game
	Script       string `json:"script,omitempty"`
	Title        string `json:"title,omitempty"`
	CommitAuthor string `json:"commit-author,omitempty"` // who makes the change
}
