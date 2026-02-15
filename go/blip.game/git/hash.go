package git

import (
	"encoding/hex"
	"time"

	"github.com/go-git/go-git/v5/plumbing"
)

type Hash = plumbing.Hash

func NewHash(s string) Hash {
	b, _ := hex.DecodeString(s)
	var h Hash
	copy(h[:], b)
	return h
}

type Commit struct {
	Hash      string
	Message   string
	CreatedAt time.Time
}

type CommitOpts struct {
	All               bool
	AllowEmptyCommits bool
}
