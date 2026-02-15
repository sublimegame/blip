package response

// Response ...
type Response struct {
	Hash                    string `json:"hash,omitempty"`
	SessionShouldBeVerified bool   `json:"ssbv,omitempty"`
	BoolValue               bool   `json:"b,omitempty"`
	Username                string `json:"username,omitempty"`
	UserID                  string `json:"userid,omitempty"`
}

// New returns an empty response
func New() *Response {
	return &Response{}
}

// GameResponse ...
type GameResponse struct {
	Script string `json:"script,omitempty"`
	Title  string `json:"title,omitempty"`
	// author
}

// Game returns an ampty game response
func Game() *GameResponse {
	return &GameResponse{}
}
