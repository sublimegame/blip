package amplitude

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"os"
)

type Channel struct {
	name string
	key  string
}

const (
	// Channel names
	channelNameProdLegacy = "prod"
	channelNameDevLegacy  = "dev"
	channelNameLink       = "link"

	amplitudeEventEndpoint = "https://api2.amplitude.com/2/httpapi"
)

var (
	ChannelProdLegacy = Channel{
		name: channelNameProdLegacy,
		key:  os.Getenv("AMPLITUDE_KEY_PROD_LEGACY"),
	}
	ChannelDevLegacy = Channel{
		name: channelNameDevLegacy,
		key:  os.Getenv("AMPLITUDE_KEY_DEV_LEGACY"),
	}
	ChannelLink = Channel{
		name: channelNameLink,
		key:  os.Getenv("AMPLITUDE_KEY_LINK"),
	}
	// Channels for Cubzh app (0.1.0+)
	ChannelProd = Channel{
		name: "prod",
		key:  os.Getenv("AMPLITUDE_KEY_PROD"),
	}
	ChannelDebug = Channel{
		name: "debug",
		key:  os.Getenv("AMPLITUDE_KEY_DEBUG"),
	}
)

// SendEvent sends on production channel by default
func SendEvent(event *Event, channel Channel) error {
	if event == nil {
		return errors.New("event is nil")
	}

	switch channel {
	case ChannelDevLegacy, ChannelProdLegacy, ChannelLink, ChannelDebug, ChannelProd:
		// continue
	default:
		return errors.New("channel is not supported")
	}

	amplitudeKeyToUse := channel.key

	// fmt.Println("[SendEvent] channel:", channel, "| amplitude API key:", amplitudeKeyToUse)
	// fmt.Println("[SendEvent] channel:", channel)

	req := &AmplitudeRequest{
		ApiKey: amplitudeKeyToUse,
		Events: []*Event{
			event,
		},
		Options: map[string]int{
			"min_id_length": 1, // UserID can have a min length of 1, now.
		},
	}

	b, err := json.Marshal(req)
	if err != nil {
		return err
	}

	// fmt.Println("AMPLITUDE EVENT:", string(b))

	resp, err := http.Post(amplitudeEventEndpoint, "application/json", bytes.NewBuffer(b))
	if err != nil {
		return err
	}

	_ /*b*/, err = io.ReadAll(resp.Body)
	defer resp.Body.Close()
	if err != nil {
		return err
	}

	// fmt.Println("RESP:", string(b))

	return err
}

type AmplitudeRequest struct {
	ApiKey  string         `json:"api_key,omitempty"`
	Events  []*Event       `json:"events,omitempty"`
	Options map[string]int `json:"options,omitempty"`
}

type Event struct {
	AppVersion         string         `json:"app_version,omitempty"`
	DeviceBrand        string         `json:"device_brand,omitempty"`
	DeviceID           string         `json:"device_id,omitempty"`
	DeviceManufacturer string         `json:"device_manufacturer,omitempty"`
	DeviceModel        string         `json:"device_model,omitempty"`
	Carrier            string         `json:"carrier,omitempty"`
	EventProperties    map[string]any `json:"event_properties,omitempty"`
	EventType          string         `json:"event_type,omitempty"`
	IP                 string         `json:"ip,omitempty"`
	Language           string         `json:"language,omitempty"`
	OSName             string         `json:"os_name,omitempty"`
	OSVersion          string         `json:"os_version,omitempty"`
	Platform           string         `json:"platform,omitempty"`
	Time               int32          `json:"time,omitempty"` // request upload time if not set
	UserID             string         `json:"user_id,omitempty"`
	UserProperties     map[string]any `json:"user_properties,omitempty"`
	SessionID          int64          `json:"session_id,omitempty"` //  The start time of the session in milliseconds since epoch

	// EventID            int32             `json:"event_id,omitempty"`
	// InsertID           string            `json:"insert_id,omitempty"`
	// Country              string            `json:"country,omitempty"`
	// Region               string            `json:"region,omitempty"`
	// City                 string            `json:"city,omitempty"`
	// DesignatedMarketArea string            `json:"dma,omitempty"`
	// Price              float32           `json:"price,omitempty"`
	// Quantity           int32             `json:"quantity,omitempty"`
	// Revenue            float32           `json:"revenue,omitempty"`
	// ProductId        string            `json:"productId,omitempty"`
}
