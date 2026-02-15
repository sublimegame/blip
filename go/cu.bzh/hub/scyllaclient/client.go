package scyllaclient

import (
	"time"

	"github.com/gocql/gocql"
	"github.com/scylladb/gocqlx/v3"
)

const (
	// Names of keyspaces
	keyspaceUniverse   = "universe"
	keyspaceModeration = "moderation"
	keyspaceKvs        = "kvs"
)

var (
	// errNotFound      error   = errors.New("not found")

	// clients, indexed by keyspace
	sharedClients map[string]*Client = make(map[string]*Client)
)

// Client ...
type Client struct {
	clusterConfig *gocql.ClusterConfig
	session       gocqlx.Session
}

type ClientConfig struct {
	Servers   []string
	Username  string
	Password  string
	AwsRegion string
	Keyspace  string
}

// Creates a new client, and return it.
func New(c ClientConfig) (*Client, error) {
	cluster := gocql.NewCluster(c.Servers...)
	if len(c.Username) > 0 && len(c.Password) > 0 {
		cluster.Authenticator = gocql.PasswordAuthenticator{Username: c.Username, Password: c.Password}
	}
	if len(c.AwsRegion) > 0 {
		cluster.PoolConfig.HostSelectionPolicy = gocql.DCAwareRoundRobinPolicy(c.AwsRegion)
	}
	if len(c.Keyspace) > 0 {
		cluster.Keyspace = c.Keyspace
	}

	// Wrap session on creation, gocqlx session embeds gocql.Session pointer.
	session, err := gocqlx.WrapSession(cluster.CreateSession())
	if err != nil {
		return nil, err
	}

	client := &Client{
		clusterConfig: cluster,
		session:       session,
	}

	return client, nil
}

// Creates a new client, store it amongst the shared clients for future access, and return it.
func NewShared(config ClientConfig) (*Client, error) {
	client, exists := sharedClients[config.Keyspace]
	if exists && client != nil {
		return client, nil
	}

	newClient, err := New(config)
	if err != nil {
		return nil, err
	}

	sharedClients[config.Keyspace] = newClient

	return newClient, nil
}

// Get shared client
func GetShared(keyspace string) *Client {
	client, exists := sharedClients[keyspace]
	if !exists {
		return nil
	}
	return client
}

// ensureClientKeyspace checks if the client is for the expected keyspace.
func (c Client) ensureClientKeyspace(expectedKeyspace string) error {
	clientKeyspace := c.clusterConfig.Keyspace
	if clientKeyspace != expectedKeyspace {
		return &errWrongKeyspace{expected: expectedKeyspace, got: clientKeyspace}
	}
	return nil
}

func TimeToInt64Ms(t time.Time) int64 {
	return t.UnixMilli()
}

func Int64MsToTime(i int64) time.Time {
	return time.UnixMilli(i)
}
