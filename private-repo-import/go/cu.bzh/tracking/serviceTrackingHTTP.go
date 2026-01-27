package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"github.com/perimeterx/marshmallow"

	"cu.bzh/amplitude"
	"cu.bzh/cors"
	httpUtils "cu.bzh/utils/http"
)

// EVENT_FIELD_BRANCH is a special field in event properties that determines
// which Amplitude channel to use for this specific event
const (
	EVENT_FIELD_BRANCH = "_branch"
)

var (
	globalTrackingChannel amplitude.Channel
)

func serveHTTP(port string, trackingChannel amplitude.Channel) {
	globalTrackingChannel = trackingChannel

	router := chi.NewRouter()

	// middlewares
	router.Use(middleware.Logger)
	router.Use(cors.GetCubzhCORSHandler())
	router.Use(middleware.Recoverer)

	// routes
	router.Get("/", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("It Works!"))
	})
	router.Post("/event", event)
	// router.Post("/event-v2", eventDumb)

	log.Fatal(http.ListenAndServe(port, router))
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
	// unix epoch when session started (provided by the server when receiving an event that's not providing it.
	SessionID int64 `json:"session_id,omitempty"`
}

type serverError struct {
	Code    int    `json:"code,omitempty"`
	Message string `json:"msg,omitempty"`
}

func respondWithError(code int, msg string, w http.ResponseWriter) {
	respondWithJSON(code, &serverError{Code: code, Message: msg}, w)
}

func respondWithJSON(code int, payload interface{}, w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(payload)
}

func respondOK(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	fmt.Fprint(w, "{}")
}

func event(w http.ResponseWriter, r *http.Request) {
	var err error
	var req eventReq

	// get client IP address
	clientIP := httpUtils.GetIP(r)

	// read the entire body
	jsonBytes, err := io.ReadAll(r.Body)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "can't read request", w)
		return
	}

	// unmarshal request JSON
	otherFields, err := marshmallow.Unmarshal(jsonBytes, &req, marshmallow.WithExcludeKnownFieldsFromMap(true))
	if err != nil {
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	branchName, branchNamefound := otherFields[EVENT_FIELD_BRANCH].(string)
	fmt.Println("🐞 BRANCH NAME [" + branchName + "]")

	// Choose Amplitude channel
	var amplitudeChannelToUse amplitude.Channel = globalTrackingChannel // legacy channel
	if branchNamefound {
		// remove from event fields
		delete(otherFields, EVENT_FIELD_BRANCH)

		if branchName == "prod" {
			amplitudeChannelToUse = amplitude.ChannelProd
		} else if branchName == "debug" {
			amplitudeChannelToUse = amplitude.ChannelDebug
		}
	}

	if req.SessionID == 0 {
		req.SessionID = -1
	}

	// extract event properties
	var eventProperties map[string]any = nil
	if len(otherFields) > 0 {
		eventProperties = make(map[string]any)
		for k, v := range otherFields {
			eventProperties[k] = v
		}
	}

	// extract user properties
	var userProperties map[string]any = make(map[string]any)
	userProperties["hw_brand"] = req.HardwareBrand
	userProperties["hw_model"] = req.HardwareModel
	userProperties["hw_product"] = req.HardwareProduct
	userProperties["hw_mem"] = req.HardwareMemory

	// construct Amplitude event
	event := &amplitude.Event{
		UserID:          req.UserID,
		DeviceID:        req.DeviceID,
		EventType:       req.Type,
		Platform:        req.Platform,
		OSName:          req.OSName,
		OSVersion:       req.OSVersion,
		AppVersion:      req.AppVersion,
		IP:              clientIP,
		EventProperties: eventProperties,
		UserProperties:  userProperties,
		SessionID:       req.SessionID,
	}

	fmt.Printf("[EVENT]: %+v:\n", event)

	// send Amplitude event
	err = amplitude.SendEvent(event, amplitudeChannelToUse)
	if err != nil {
		fmt.Println("amplitude error:", err.Error())
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	respondOK(w)
}

// New route handler for the event endpoint (dumb version).
// This version trusts
// func eventDumb(w http.ResponseWriter, r *http.Request) {

// 	var err error
// 	var req map[string]any

// 	// get client IP address
// 	// clientIP := httpUtils.GetIP(r)

// 	err = json.NewDecoder(r.Body).Decode(&req)
// 	if err != nil {
// 		respondWithError(http.StatusInternalServerError, "can't decode json request", w)
// 		return
// 	}

// 	// ...

// 	respondWithError(http.StatusInternalServerError, "not implemented", w)
// }
