package types

import (
	"errors"
	"time"
)

type Redirect struct {
	Hash            string    `json:"hash"`
	Destination     string    `json:"dest"`
	CreatorUsername string    `json:"creator"`
	UpdaterUsername string    `json:"updater"`
	Created         time.Time `json:"created"`
	Updated         time.Time `json:"updated"`
}

func NewRedirect(hash, destination, creatorUsername string) Redirect {
	return Redirect{
		Hash:            hash,
		Destination:     destination,
		CreatorUsername: creatorUsername,
		UpdaterUsername: creatorUsername,
		Created:         time.Now(),
		Updated:         time.Now(),
	}
}

type Config struct {
	Redirects map[string]Redirect
}

func NewConfig() *Config {
	return &Config{
		Redirects: make(map[string]Redirect),
	}
}

// CreateRedirect adds a new Redirect to the config.
// If the hash is already used, an error is returned.
func (c *Config) CreateRedirect(r Redirect) error {
	_, exists := c.Redirects[r.Hash]
	if exists {
		return errors.New("a redirect using this hash already exists")
	}
	c.Redirects[r.Hash] = r
	return nil
}

// UpdateRedirect updates an existing redirect
// Returns an error if the provided Redirect doesn't exist
func (c *Config) UpdateRedirect(r Redirect) error {
	_, exists := c.Redirects[r.Hash]
	if !exists {
		return errors.New("no redirect found for this hash")
	}
	c.Redirects[r.Hash] = r
	return nil
}

// DeleteRedirect removes a redirect from the config
// Returns an error if the provided Redirect doesn't exist
func (c *Config) DeleteRedirect(r Redirect) error {
	_, exists := c.Redirects[r.Hash]
	if !exists {
		return errors.New("no redirect found for this hash")
	}
	delete(c.Redirects, r.Hash)
	return nil
}

func (r *Redirect) Validate() error {
	if r.Hash == "" {
		return errors.New("hash cannot be empty")
	}
	if r.Destination == "" {
		return errors.New("destination cannot be empty")
	}
	if r.CreatorUsername == "" {
		return errors.New("creator username cannot be empty")
	}
	return nil
}
