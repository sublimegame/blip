package main

import (
	"context"
	"errors"
	"time"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"

	"hubserver/user"
	"mailer"
	"utils"
)

//
func connect() (*mongo.Client, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	client, err := mongo.Connect(ctx, options.Client().ApplyURI("mongodb://user-db:27017"))
	if err != nil {
		return nil, err
	}
	return client, nil
}

//
func isEmailAlreadyUsed(client *mongo.Client, email string) (bool, error) {
	if client == nil {
		return false, errors.New("mongo client is nil")
	}
	if len(email) == 0 {
		return false, errors.New("email is empty")
	}
	collection := client.Database("particubes").Collection("users")
	usr, err := user.GetByEmail(collection, email)
	if err != nil {
		return false, err
	}
	return (usr != nil), nil
}

//
func createAccountForBetaTester(client *mongo.Client, email string) (string, error) {
	if client == nil {
		return "", errors.New("mongo client is nil")
	}
	if len(email) == 0 {
		return "", errors.New("email is empty")
	}

	newUser, err := user.New()
	if err != nil {
		return "", err
	}

	uuid, err := utils.UUID()
	if err != nil {
		return "", err
	}

	newUser.Email = email
	newUser.EmailConfirmHash = uuid

	collection := client.Database("particubes").Collection("users")
	err = newUser.Save(collection)
	return uuid, err
}

//
func sendEmailAddressConfirmationEmail(emailAddress, templatePath, emailContentMarkdownPath, confirmationHash string) error {

	var content struct {
		Link string
	}

	content.Link = "https://particubes.com/confirm/" + confirmationHash

	err := mailer.Send(
		emailAddress,
		"", // email address used as name, because we don't have it
		templatePath,
		emailContentMarkdownPath,
		MAILJET_API_KEY,
		MAILJET_API_SECRET,
		&content,
		"particubes-email-confirm",
	)
	return err
}

// confirmEmailAddress ...
func confirmEmailAddress(client *mongo.Client, confirmHash string) (bool, error) {
	collection := client.Database("particubes").Collection("users")
	usr, err := user.GetByEmailConfirmHash(collection, confirmHash)
	if err != nil {
		return false, err
	}
	if usr == nil {
		return false, nil
	}

	// email already confirmed
	if usr.EmailConfirmed {
		return true, nil
	}

	now := time.Now()
	usr.Updated = &now
	usr.LastSeen = &now
	err = usr.ConfirmEmail(collection)
	return true, err
}

// getUserCount ...
func getUserCount(client *mongo.Client) (int64, error) {
	collection := client.Database("particubes").Collection("users")
	opts := options.Count().SetMaxTime(5 * time.Second)
	count, err := collection.CountDocuments(context.TODO(), bson.D{}, opts)
	return count, err
}
