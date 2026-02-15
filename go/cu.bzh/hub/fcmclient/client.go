package fcmclient

import (
	"context"
	"errors"
	"fmt"
	"log"
	"sync"

	firebase "firebase.google.com/go/v4"
	"firebase.google.com/go/v4/messaging"
	"google.golang.org/api/option"
)

const (
	FIREBASE_PROJECT_ID                = "cubzh-25be5"
	FIREBASE_ACCOUNT_JSON_DEFAULT_PATH = "/cubzh/secrets/firebase/cubzh-firebase.json"
)

var (
	sharedInstanceOnce sync.Once
	sharedInstance     *Client = nil
)

// ------------------------------
// Type fcmclient.Client
// ------------------------------

type Client struct {
	app *firebase.App
}

type ClientOpts struct {
	// Path to the Firebase account JSON file
	// If not provided, the default path will be used
	FirebaseAccountJSONFilePath string
}

func (c *Client) init(opts ClientOpts) error {

	// if FirebaseAccountJSONFilePath is not provided, use the default one
	if opts.FirebaseAccountJSONFilePath == "" {
		opts.FirebaseAccountJSONFilePath = FIREBASE_ACCOUNT_JSON_DEFAULT_PATH
	}

	// Retrieve the Firebase account JSON content
	opt := option.WithCredentialsFile(opts.FirebaseAccountJSONFilePath)
	if opt == nil {
		return errors.New("no Firebase account JSON provided")
	}

	// Initialize the Firebase app
	config := &firebase.Config{
		ProjectID: FIREBASE_PROJECT_ID,
	}
	app, err := firebase.NewApp(context.Background(), config, opt)
	if err != nil || app == nil {
		return err
	}
	c.app = app

	return nil // success
}

// SendNotification sends a notification to a user
func (c *Client) SendNotification(clientToken, category, title, body, notificationID, sound string, badgeCount int) error {
	// Obtain a messaging.Client from the App.
	ctx := context.Background()
	client, err := c.app.Messaging(ctx)
	if err != nil {
		fmt.Println("❌ [FCM] error getting Messaging client:", err.Error())
		return err
	}

	// This registration token comes from the client FCM SDKs.
	registrationToken := clientToken

	// See documentation on defining a message payload.
	message := &messaging.Message{
		Notification: &messaging.Notification{
			Title: title,
			Body:  body,
		},
		Android: &messaging.AndroidConfig{
			Priority: "normal", // can be delayed
			Notification: &messaging.AndroidNotification{
				Sound:             "default",
				NotificationCount: &badgeCount,
			},
		},
		Data: map[string]string{
			"category":       category,
			"notificationID": notificationID,
		},
		Token: registrationToken,
	}

	if sound != "" {
		message.Android.Notification.Sound = sound
	}

	// for custom Android notif sounds (when app is in background)
	if category == "money" {
		message.Android.Notification.ChannelID = "money"
	} else {
		message.Android.Notification.ChannelID = "generic"
	}

	// Send a message to the device corresponding to the provided
	// registration token.
	_ /* response */, err = client.Send(ctx, message)
	if err != nil {
		fmt.Println("❌ [FCM] error sending message to client:", err.Error())
		return err
	}

	// Response is a message ID string.
	// fmt.Println("[FCM] Successfully sent message:", response)
	return nil
}

// ------------------------------
// Shared instance
// ------------------------------

func Shared(opts ClientOpts) *Client {
	// init shared instance only once
	sharedInstanceOnce.Do(func() {
		sharedInstance = &Client{}
		err := sharedInstance.init(opts)
		if err != nil {
			log.Fatalln("❌ ERROR: failed to create Firebase client (crashing): ", err.Error())
		}
	})
	return sharedInstance
}
