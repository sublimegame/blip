package main

import (
	"fmt"
	"log"
	"os"
)

const (
	issuerID = "da974e40-148f-4e7f-8bcd-74a73d36e1d1"
	keyID    = "M28VDQ8T63"
	keyFile  = "AuthKey_M28VDQ8T63.p8"
)

func main() {
	if len(os.Args) < 2 {
		log.Fatal("Please provide a command: list-apps or download-metadata <app-id>")
	}

	client, err := NewClient(issuerID, keyID, keyFile)
	if err != nil {
		log.Fatal(err)
	}

	command := os.Args[1]

	switch command {
	case "list-apps":
		apps, err := client.ListApps()
		if err != nil {
			log.Fatal(err)
		}
		for _, app := range apps {
			fmt.Printf("ID: %s, Name: %s\n", app.ID, app.Name)
		}
	case "get-localizations":
		if len(os.Args) < 3 {
			log.Fatal("Please provide an app ID")
		}
		appID := os.Args[2]
		localizations, err := client.GetLocalizations(appID)
		if err != nil {
			log.Fatal(err)
		}
		for _, localization := range localizations {
			fmt.Printf("# %s\n", localization.Locale)
			fmt.Printf("ID: %s\n", localization.ID)
			fmt.Printf("Name: %s\n", localization.Name)
			fmt.Printf("Subtitle: %s\n", localization.Subtitle)
			fmt.Printf("Privacy Policy URL: %s\n", localization.PrivacyPolicyURL)
			fmt.Printf("---------\n")
		}
	default:
		log.Fatal("Unknown command. Use list-apps or download-metadata <app-id>")
	}
}
