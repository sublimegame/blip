package apnsclient

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"sync"

	"github.com/sideshow/apns2"
	"github.com/sideshow/apns2/token"
)

const (
	PUSH_APPLE_TOKEN_FILEPATH = "/cubzh/secrets/apns/AuthKey_X48M5UN4U7.p8"
)

var (
	sharedInstanceOnce sync.Once
	sharedInstance     *Client = nil
)

// ------------------------------
// Type apnsclient.Client
// ------------------------------

type Client struct {
	apnsClientProd *apns2.Client
	apnsClientDev  *apns2.Client
}

type ClientOpts struct {
	// Path to the APNS auth key file
	// If not provided, the default path will be used
	APNSAuthKeyFilepath string
}

func (c *Client) init(opts ClientOpts) error {

	// if APNSAuthKeyFilepath is not provided, use the default one
	if opts.APNSAuthKeyFilepath == "" {
		opts.APNSAuthKeyFilepath = PUSH_APPLE_TOKEN_FILEPATH
	}

	// create apns clients if they don't exist
	if c.apnsClientProd == nil || c.apnsClientDev == nil {

		authKey, err := token.AuthKeyFromFile(opts.APNSAuthKeyFilepath)
		if err != nil {
			return err
		}

		token := &token.Token{
			AuthKey: authKey,
			// KeyID from developer account (Certificates, Identifiers & Profiles -> Keys)
			KeyID: "X48M5UN4U7",
			// TeamID from developer account (View Account -> Membership)
			TeamID: "9JFN8QQG65",
		}

		// Create 2 clients: one for production tokens, one for development ones
		{
			c.apnsClientProd = apns2.NewTokenClient(token)
			if c.apnsClientProd == nil {
				return errors.New("failed to create APNS client (Production)")
			}
			c.apnsClientProd.Production()
		}
		{
			c.apnsClientDev = apns2.NewTokenClient(token)
			if c.apnsClientDev == nil {
				return errors.New("failed to create APNS client (Development)")
			}
			c.apnsClientDev.Development()
		}
	}
	return nil // success
}

// Apple
type alert struct {
	Title    string `json:"title,omitempty"`
	Subtitle string `json:"subtitle,omitempty"`
	Body     string `json:"body,omitempty"`
}
type aps struct {
	Alert alert  `json:"alert"`
	Badge int    `json:"badge"`
	Sound string `json:"sound"`
}
type appleNotification struct {
	// Apple fields
	Aps aps `json:"aps"`
	// user-defined fields
	NotificationID string `json:"notificationID"`
	Category       string `json:"category"`
	// other user-defined fields
}

func (c *Client) SendNotification(clientToken, tokenVariant, category, title, body, notificationID, sound string, badgeCount uint) error {

	const APP_BUNDLE_ID = "com.voxowl.blip"

	// Construct notification
	var notif appleNotification
	notif.Category = category
	notif.Aps.Alert.Title = title
	notif.Aps.Alert.Body = body
	notif.NotificationID = notificationID
	notif.Aps.Sound = sound
	notif.Aps.Badge = int(badgeCount)

	notifJSON, err := json.Marshal(notif)
	if err != nil {
		return err
	}
	notification := &apns2.Notification{
		DeviceToken: clientToken,
		Topic:       APP_BUNDLE_ID,
		Payload:     notifJSON,
	}

	// Choose APNS client to use (dev or prod)
	// Note: old tokens don't have any variant value, so we default to prod
	apnsClientToUse := c.apnsClientProd
	if tokenVariant == "dev" {
		apnsClientToUse = c.apnsClientDev
		fmt.Println("🐞[PushNotif][APNS] Sending notification (dev)")
	} else {
		fmt.Println("🐞[PushNotif][APNS] Sending notification (prod)")
	}

	// Send notification using the APNS client
	res, err := apnsClientToUse.Push(notification)
	if err != nil {
		return err
	}
	if res.StatusCode != 200 {
		return errors.New("response status code is not 200 (" + res.Reason + ")")
	}

	return nil // success
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
			log.Fatalln("❌ ERROR: failed to create APNS client (crashing): ", err.Error())
		}
	})
	return sharedInstance
}
