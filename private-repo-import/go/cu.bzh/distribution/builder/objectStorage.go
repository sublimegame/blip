package main

import (
	"context"
	"errors"
	"fmt"
	"io"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/credentials"
	"github.com/aws/aws-sdk-go-v2/service/s3"
)

// ObjectStorage is a generic interface for interacting with object storage services
type ObjectStorage interface {
	// Connect to the object storage service
	// Connect() error

	// Upload a file to the object storage service
	UploadFile(name string, contentReadSeeker io.ReadSeeker) error

	// Download a file from the object storage service
	// DownloadFile() error

	// List files
	ListFiles(path string) ([]string, error)
}

// DigitalOceanObjectStorage is an implementation of the ObjectStorage interface for DigitalOcean Spaces Object Storage.
// It represents a single bucket.
type DigitalOceanObjectStorage struct {
	region   string
	bucket   string
	s3Client *s3.Client
}

func NewDigitalOceanObjectStorage(authKey, authSecret, region, bucket string) (DigitalOceanObjectStorage, error) {

	newS3Client := s3.New(s3.Options{
		Region:       region,
		BaseEndpoint: aws.String("https://" + region + ".digitaloceanspaces.com"),
		Credentials:  credentials.NewStaticCredentialsProvider(authKey, authSecret, ""),
		UsePathStyle: true,
	})
	if newS3Client == nil {
		return DigitalOceanObjectStorage{}, errors.New("failed to create s3 client")
	}

	objStorage := DigitalOceanObjectStorage{
		region:   region,
		bucket:   bucket,
		s3Client: newS3Client,
	}

	return objStorage, nil
}

// func (s DigitalOceanObjectStorage) UploadFile(name string, content []byte) error {

// 	// Define the parameters of the object you want to upload.
// 	object := s3.PutObjectInput{
// 		Bucket: aws.String(s.bucket),     // The path to the directory you want to upload the object to, starting with your Space name.
// 		Key:    aws.String(name),         // Object key, referenced whenever you want to access this file later.
// 		Body:   bytes.NewReader(content), // The object's contents.
// 		// ACL:    aws.String("private"),    // Defines Access-control List (ACL) permissions, such as private or public.
// 		// Metadata: map[string]*string{ // Required. Defines metadata tags.
// 		// 	"x-amz-meta-my-key": aws.String("your-value"),
// 		// },
// 	}

// 	// Run the PutObject function with your parameters, catching for errors.
// 	_, err := s.s3Client.PutObject(&object)

// 	return err
// }

func (s DigitalOceanObjectStorage) UploadFile(name string, contentReadSeeker io.ReadSeeker) error {
	fmt.Println("🐞 [UploadFile] name:", name)
	fmt.Println("   [UploadFile] bucket:", s.bucket)
	fmt.Println("   [UploadFile] region:", s.region)

	// Define the parameters of the object you want to upload.
	input := &s3.PutObjectInput{
		Bucket: aws.String(s.bucket),
		Key:    aws.String(name),
		Body:   contentReadSeeker,
	}

	// Run the PutObject function with your parameters, catching for errors.
	output, err := s.s3Client.PutObject(context.TODO(), input)
	if output != nil {
		fmt.Println("   [UploadFile] output:", *output)
	}
	if err != nil {
		return err
	}

	return err
}

func (s DigitalOceanObjectStorage) ListFiles(path string) ([]string, error) {
	return nil, errors.New("not implemented")
}
