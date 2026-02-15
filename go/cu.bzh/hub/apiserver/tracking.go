package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"

	httpUtils "cu.bzh/utils/http"
)

type Trackin struct {
	Server *WorldServer `json:"server,omitempty"`
}

type eventReq struct {
	UserID          string `json:"user-id,omitempty"`
	DeviceID        string `json:"device-id,omitempty"`
	Type            string `json:"type,omitempty"`
	Platform        string `json:"platform,omitempty"`
	OSName          string `json:"os-name,omitempty"`
	OSVersion       string `json:"os-version,omitempty"`
	AppVersion      string `json:"app-version,omitempty"`
	HardwareBrand   string `json:"hw-brand,omitempty"`
	HardwareModel   string `json:"hw-model,omitempty"`
	HardwareProduct string `json:"hw-product,omitempty"`
	HardwareMemory  int32  `json:"hw-mem,omitempty"`
}

func trackEventForHTTPReq(req *eventReq, r *http.Request) {
	session, err := getSessionFromRequestContext(r)
	if err == nil && session != nil {
		req.DeviceID = session.ID
	}
	trackEvent(req, httpUtils.GetIP(r))
}

func trackEvent(r *eventReq, senderIP string) {

	go func(r *eventReq, senderIP string) {

		jsonData, err := json.Marshal(r)
		if err != nil {
			fmt.Println("❌ trackEvent (1):", err.Error())
			return
		}

		req, err := http.NewRequest("POST", "https://debug.cu.bzh/event", bytes.NewBuffer(jsonData))
		if err != nil {
			fmt.Println("❌ trackEvent (2):", err.Error())
			return
		}

		req.Header.Set("Content-Type", "application/json")
		req.Header.Set("X-Forwarded-For", senderIP)

		client := &http.Client{}
		resp, err := client.Do(req)
		if err != nil {
			fmt.Println("❌ trackEvent (3):", err.Error())
			return
		}
		defer resp.Body.Close()

		if resp.StatusCode != http.StatusOK {
			fmt.Println("❌ trackEvent (4): status:", resp.Status)
			return
		}

	}(r, senderIP)
}
