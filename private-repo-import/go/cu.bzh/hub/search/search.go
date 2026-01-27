package search

import (
	"context"
	"errors"
	"time"

	typesense "github.com/typesense/typesense-go/typesense"
)

const (
	COLLECTION_ITEMDRAFTS string = "ItemDrafts"
	COLLECTION_WORLDS     string = "Worlds"
	COLLECTION_CREATIONS  string = "Creations"
)

var (
	ErrDocumentIsNil error = errors.New("document is nil")
)

// Client ...
type Client struct {
	TSClient *typesense.Client
}

func NewClient(serverAddr, apiKey string) (*Client, error) {
	c := &Client{}
	c.TSClient = typesense.NewClient(
		typesense.WithServer(serverAddr),
		typesense.WithAPIKey(apiKey),
		typesense.WithConnectionTimeout(5*time.Second),
		// typesense.WithCircuitBreakerMaxRequests(50),
		// typesense.WithCircuitBreakerInterval(2*time.Minute),
		// typesense.WithCircuitBreakerTimeout(1*time.Minute),
	)
	if c.TSClient == nil {
		return nil, errors.New("failed to init typesense client")
	}
	return c, nil
}

func (c *Client) CollectionExists(collectionName string) bool {
	_, err := c.TSClient.Collection(collectionName).Retrieve(context.Background())
	return err == nil
}

func (c *Client) CollectionDelete(collectionName string) error {
	_, err := c.TSClient.Collection(collectionName).Delete(context.Background())
	return err
}

// --------------------------------------------------
// Unexported functions
// --------------------------------------------------

func newTypeAssertErr(fieldName string) error {
	return errors.New("type assertion error for '" + fieldName + "'")
}
