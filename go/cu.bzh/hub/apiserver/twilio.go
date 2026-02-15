package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"

	twilio "github.com/twilio/twilio-go"
	twilioApi "github.com/twilio/twilio-go/rest/api/v2010"
)

var (
	TWILIO_ACCOUNT_SID        string = "" // loaded from env
	TWILIO_CUBZH_PHONE_NUMBER string = "" // loaded from env
)

var (
	SECRET_TWILIO_API_AUTH_TOKEN string = "" // loaded from filesystem
)

type TwilioPhoneNumberInfo struct {
	IsValid     bool
	PhoneNumber string
	CountryCode string
	// ...
}

type TwilioResponse struct {
	CountryCode       *string  `json:"country_code"`
	PhoneNumber       *string  `json:"phone_number"`
	Valid             *bool    `json:"valid"`
	validation_errors []string `json:"validation_errors"`
}

// locks
func twilioSanitizePhoneNumberWithAPI(phone string) (TwilioPhoneNumberInfo, error) {
	var phoneInfo TwilioPhoneNumberInfo

	// Define the URL and authentication credentials
	url := "https://lookups.twilio.com/v2/PhoneNumbers/" + phone
	sid := TWILIO_ACCOUNT_SID
	token := SECRET_TWILIO_API_AUTH_TOKEN

	// Send request to Twilio API
	// Create a new HTTP request
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		fmt.Println("Error creating request:", err)
		return phoneInfo, err
	}

	// Set basic authentication headers
	req.SetBasicAuth(sid, token)

	// Create an HTTP client and send the request
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		fmt.Println("Error sending request:", err)
		return phoneInfo, err
	}

	// Check if the response status is not 200 OK
	if resp.StatusCode != http.StatusOK {
		fmt.Printf("Error: received non-200 response status: %d %s\n", resp.StatusCode, resp.Status)
		return phoneInfo, errors.New("non-200 response status")
	}

	// Parse the JSON response
	var response TwilioResponse
	err = json.NewDecoder(resp.Body).Decode(&response)
	if err != nil {
		fmt.Println("Error parsing JSON response:", err)
		return phoneInfo, err
	}

	// // Print the formatted JSON response
	// prettyJSON, err := json.MarshalIndent(result, "", "  ")
	// if err != nil {
	// 	fmt.Println("Error formatting JSON:", err)
	// 	return phoneInfo, err
	// }
	// fmt.Println(string(prettyJSON))

	// {
	// 	"call_forwarding": null,
	// 	"caller_name": null,
	// 	"calling_country_code": "33",
	// 	"country_code": "FR",
	// 	"identity_match": null,
	// 	"line_status": null,
	// 	"line_type_intelligence": null,
	// 	"national_format": "07 66 35 14 91",
	// 	"phone_number": "+33766351491",
	// 	"phone_number_quality_score": null,
	// 	"pre_fill": null,
	// 	"reassigned_number": null,
	// 	"sim_swap": null,
	// 	"sms_pumping_risk": null,
	// 	"url": "https://lookups.twilio.com/v2/PhoneNumbers/+33766351491",
	// 	"valid": true,
	// 	"validation_errors": []
	// }

	// "valid": false,
	// "validation_errors": [
	// 		"TOO_SHORT"
	// ]

	if response.PhoneNumber != nil {
		phoneInfo.PhoneNumber = *response.PhoneNumber
	}

	if response.CountryCode != nil {
		phoneInfo.CountryCode = *response.CountryCode
	}

	if response.Valid != nil {
		phoneInfo.IsValid = *response.Valid
	}

	return phoneInfo, nil
}

func twilioSendDashboardSMSToParent(parentPhoneNumber, dashboardURL string) error {

	client := twilio.NewRestClientWithParams(twilio.ClientParams{
		Username: TWILIO_ACCOUNT_SID,
		Password: SECRET_TWILIO_API_AUTH_TOKEN,
	})

	params := &twilioApi.CreateMessageParams{}
	params.SetTo(parentPhoneNumber)
	params.SetFrom(TWILIO_CUBZH_PHONE_NUMBER)
	params.SetBody(constructSMSForParentDashboard(dashboardURL))

	resp, err := client.Api.CreateMessage(params)
	if err != nil {
		fmt.Println("🐞[Twilio] Error sending SMS message: " + err.Error())
	} else {
		response, _ := json.Marshal(*resp)
		fmt.Println("🐞[Twilio] Response: " + string(response))
	}

	return err
}
