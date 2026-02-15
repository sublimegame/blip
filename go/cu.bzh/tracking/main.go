package main

import (
	"fmt"
	"os"

	"cu.bzh/amplitude"
)

// Environment variables:
//   CZH_TRACK_CHANNEL: Determines which Amplitude channel to use
//     - "dev": Development channel
//     - "prod": Production channel
//

const (
	ENV_VAR_TRACK_CHANNEL = "CZH_TRACK_CHANNEL"
	LISTEN_ADDR           = ":80" // HTTP port to listen on

	CHANNEL_DEV  = "dev"
	CHANNEL_PROD = "prod"
)

func main() {
	fmt.Println("✨ Starting tracking server...")

	envTrackChannel := os.Getenv(ENV_VAR_TRACK_CHANNEL)
	if envTrackChannel == "" {
		fmt.Printf("env var missing: %s. Stopping.\n", ENV_VAR_TRACK_CHANNEL)
		os.Exit(1)
	}
	if envTrackChannel != CHANNEL_PROD && envTrackChannel != CHANNEL_DEV {
		fmt.Printf("env var value is invalid: %s can only be \"%s\" or \"%s\". Stopping.\n",
			ENV_VAR_TRACK_CHANNEL, CHANNEL_DEV, CHANNEL_PROD)
		os.Exit(1)
	}

	var amplitudeChannel amplitude.Channel = amplitude.ChannelDevLegacy
	if envTrackChannel == CHANNEL_PROD {
		amplitudeChannel = amplitude.ChannelProdLegacy
	}

	fmt.Printf("📡 Channel: %s \n", envTrackChannel)
	fmt.Printf("🌍 Listen: %s\n", LISTEN_ADDR)

	serveHTTP(LISTEN_ADDR, amplitudeChannel)
}
