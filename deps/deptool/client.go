package deptool

import (
	"fmt"
	"os"

	"github.com/voxowl/objectstorage"
	"github.com/voxowl/objectstorage/digitalocean"
)

const (
	defaultDigitalOceanSpacesRegion = "nyc3"
	defaultDigitalOceanSpacesBucket = "cubzh-deps"
)

func getDefaultDigitalOceanSpacesAuthKey() string {
	return os.Getenv("DIGITALOCEAN_SPACES_AUTH_KEY")
}

func getDefaultDigitalOceanSpacesAuthSecret() string {
	return os.Getenv("DIGITALOCEAN_SPACES_AUTH_SECRET")
}

type ObjectStorageBuildFunc func() (objectstorage.ObjectStorage, error)

type DigitalOceanObjectStorageClientOpts struct {
	AuthKey    string // optional
	AuthSecret string // optional
	Region     string // optional
	Bucket     string // optional
}

func NewDigitalOceanObjectStorageClient(opts DigitalOceanObjectStorageClientOpts) (objectstorage.ObjectStorage, error) {

	// if only one of the two is set, return an error
	if (opts.AuthKey == "" && opts.AuthSecret != "") || (opts.AuthKey != "" && opts.AuthSecret == "") {
		return nil, fmt.Errorf("opts.AuthKey and opts.AuthSecret must be both set (or not set at all)")
	}

	// credentials
	authKey := opts.AuthKey
	authSecret := opts.AuthSecret
	if authKey == "" && authSecret == "" {
		authKey = getDefaultDigitalOceanSpacesAuthKey()
		authSecret = getDefaultDigitalOceanSpacesAuthSecret()
	}

	// region & bucket
	region := opts.Region
	bucket := opts.Bucket
	if region == "" {
		region = defaultDigitalOceanSpacesRegion
	}
	if bucket == "" {
		bucket = defaultDigitalOceanSpacesBucket
	}

	// Create the object storage client
	objectStorageClient, err := digitalocean.NewDigitalOceanObjectStorage(
		digitalocean.DigitalOceanConfig{
			Region:     region,
			Bucket:     bucket,
			AuthKey:    authKey,
			AuthSecret: authSecret,
		},
		digitalocean.DigitalOceanObjectStorageOpts{
			UsePathStyle: true,
		},
	)
	return objectStorageClient, err
}
