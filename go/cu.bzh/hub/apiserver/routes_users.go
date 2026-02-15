package main

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"net/http"
	"slices"

	"cu.bzh/hub/dbclient"
	"cu.bzh/utils"
	"github.com/fxamacker/cbor"
)

const (
	SESSION_FIELD_PASSKEY_CHALLENGE = "passkeyChallenge"

	// How to generate the following hashes:
	// 1) keytool -list -v -keystore /Users/gaetan/projects/voxowl/particubes-private/pcubes-keystore.jks -alias release
	// 2) get SHA-256 value and remove all ':'
	// 3) echo -n "<value>" | xxd -r -p | base64 | tr +/ -_ | tr -d =
	ANDROID_PROD_RELEASE_KEY_HASH = "oTE-FPD1lct6R6VOaY5ERV-DwlQr9gNSLMkL_Bx6OOI"
	ANDROID_DEV_DEBUG_KEY_HASH    = "gp5fbrMM50ugOjFrMmzrq6GVM2srhZhkfMFeNkG9gE4"
	ANDROID_DEV_RELEASE_KEY_HASH  = "t9uF55we9U2_XcsdaCEXsQqK-QEPKUNq_GYgWWokpEo"
)

var (
	allowedOrigins = []string{
		"https://" + PASSKEY_RP_ID,                              // https://blip.game
		"android:apk-key-hash:" + ANDROID_PROD_RELEASE_KEY_HASH, // Android (prod release)
		"android:apk-key-hash:" + ANDROID_DEV_DEBUG_KEY_HASH,    // Android (dev debug)
		"android:apk-key-hash:" + ANDROID_DEV_RELEASE_KEY_HASH,  // Android (dev release)
	}
)

// getUserPasskeyChallenge generates a new passkey challenge,
// stores it in the sever-side session (NOT in the database),
// and returns it to the client.
func getUserPasskeyChallenge(w http.ResponseWriter, r *http.Request) {

	clientInfo, err := getClientInfo(r)
	if err != nil {
		fmt.Println("❌ [getUserPasskeyChallenge] failed to get client info:", err)
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// check if client build number is >= 208
	if clientInfo.BuildNumber < 208 {
		fmt.Println("❌ [getUserPasskeyChallenge] client is too old")
		respondWithError(http.StatusBadRequest, "client is too old", w)
		return
	}

	newChallenge := utils.RandomAlphaNumericString(32)

	// store the challenge in the server-side session
	session, err := getSessionFromRequestContext(r)
	if err != nil {
		fmt.Println("❌ [getUserPasskeyChallenge] failed to get session from request context:", err)
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	session.Values[SESSION_FIELD_PASSKEY_CHALLENGE] = newChallenge

	type getUserPasskeyChallengeRes struct {
		Challenge string `json:"challenge"`
	}

	res := &getUserPasskeyChallengeRes{
		Challenge: newChallenge,
	}

	respondWithJSON(http.StatusOK, res, w)
}

func postUserPasskey(w http.ResponseWriter, r *http.Request) {

	fmt.Println("🔑 [postUserPasskey] 1")

	// retrieve client info
	clientInfo, err := getClientInfo(r)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to get client info:", err)
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// retrieve challenge from the server-side session
	challengeStr, err := getSessionFieldPasskeyChallenge(r)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to get challenge from session:", err)
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// retrieve user sending the request
	usr, err := getUserFromRequestContext(r)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to get user from request context:", err)
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	// check if client build number is >= 208
	if clientInfo.BuildNumber < 208 {
		fmt.Println("❌ [postUserPasskey] client is too old")
		respondWithError(http.StatusBadRequest, "client is too old", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 2")

	// parse request

	type PostPasskeyRequest struct {
		ID       string `json:"id"`
		RawID    string `json:"rawId"`
		Type     string `json:"type"`
		Response struct {
			ClientDataJSON    string `json:"clientDataJSON"`
			AttestationObject string `json:"attestationObject"`
		} `json:"response"`
	}

	var req PostPasskeyRequest
	jsonDecoder := json.NewDecoder(r.Body)
	// Android sends a full JSON object, with fields that are not in the struct.
	// We don't want to disallow unknown fields, for the decoding to succeed.
	// jsonDecoder.DisallowUnknownFields()
	err = jsonDecoder.Decode(&req)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to decode json request:", err)
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 3", req)

	// check if the request is valid
	if req.ID == "" || req.RawID == "" || req.Type == "" || req.Response.ClientDataJSON == "" || req.Response.AttestationObject == "" {
		fmt.Println("❌ [postUserPasskey] invalid request")
		respondWithError(http.StatusBadRequest, "invalid request", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 4")

	// Decode client data JSON

	clientDataBytes, err := base64.RawURLEncoding.DecodeString(req.Response.ClientDataJSON)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to decode client data:", err)
		respondWithError(http.StatusBadRequest, "failed to decode client data", w)
		return
	}

	var clientData PasskeyClientData
	err = json.Unmarshal(clientDataBytes, &clientData)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to unmarshal client data:", err)
		respondWithError(http.StatusBadRequest, "failed to unmarshal client data", w)
		return
	}

	// check Type field has the expected value
	if clientData.Type != "webauthn.create" {
		fmt.Println("❌ [postUserPasskey] invalid client data type:", clientData.Type)
		respondWithError(http.StatusBadRequest, "invalid client data type", w)
		return
	}

	// check challenge field has the same value as the challenge sent earlier to the client
	expectedChallengeBase64 := base64.RawURLEncoding.EncodeToString([]byte(challengeStr))
	if clientData.Challenge != expectedChallengeBase64 {
		fmt.Println("❌ [postUserPasskey] invalid challenge:", clientData.Challenge, "expected:", expectedChallengeBase64)
		respondWithError(http.StatusBadRequest, "invalid challenge", w)
		return
	}

	// check origin field has the expected value
	if !slices.Contains(allowedOrigins, clientData.Origin) {
		fmt.Println("❌ [postUserPasskey] invalid origin:", clientData.Origin)
		respondWithError(http.StatusBadRequest, "invalid origin", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 5")

	// Decode attestation object

	// base64 decoding
	attestationObjectBytes, err := base64.RawURLEncoding.DecodeString(req.Response.AttestationObject)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to decode attestation object:", err)
		respondWithError(http.StatusBadRequest, "failed to decode attestation object", w)
		return
	}

	// CBOR unmarshal
	var attObj struct {
		AuthData []byte            `cbor:"authData"`
		AttStmt  map[string][]byte `cbor:"attStmt"`
		Fmt      string            `cbor:"fmt"`
	}

	err = cbor.Unmarshal(attestationObjectBytes, &attObj)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to unmarshal attestation object:", err)
		respondWithError(http.StatusBadRequest, "failed to unmarshal attestation object", w)
		return
	}

	// For now, we only support "none" format.
	// We might have to support other formats in the future.
	// Verify attestation.
	// Depending on the attestation format (fmt in attStmt), you may need to validate a signature.
	// For example, if fmt is "packed", retrieve the signature and certificate chain from attStmt["sig"] and attStmt["x5c"],
	// then verify the signature over authData||SHA256(clientDataJSON).
	// If fmt is "none", no attestation check is done (less secure).
	// Follow steps 9–14 of the WebAuthn spec to fully verify the attestation object (many details are in RFCs),
	// but at minimum ensure the data hasn’t been tampered and the attestation signature is valid if present.
	// (For a simple start, you might accept "none" attestation or skip verifying certificates, but always validate challenge and RP ID.)
	if attObj.Fmt != "none" {
		fmt.Println("❌ [postUserPasskey] invalid format:", attObj.Fmt)
		respondWithError(http.StatusBadRequest, "invalid format", w)
		return
	}

	// Parse authData

	credData, signCount, err := parseAuthData(attObj.AuthData, "blip.game")
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to parse authData:", err)
		respondWithError(http.StatusBadRequest, "failed to parse authData", w)
		return
	}

	// encode AAGUID to base64 (URL-safe)
	aaguidBase64 := base64.RawURLEncoding.EncodeToString(credData.AAGUID)
	// encode credentialID to base64 (URL-safe)
	credentialIDBase64 := base64.RawURLEncoding.EncodeToString(credData.CredentialID)
	// encode public key to PEM base64
	pubKeyPEM, err := decodeCOSEKeyToPEM(credData.CredentialPublicKey)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to decode COSE key to PEM:", err)
		respondWithError(http.StatusBadRequest, "failed to decode COSE key to PEM", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 6")

	usr.PasskeyAAGUID = aaguidBase64
	usr.PasskeyCredentialID = credentialIDBase64
	usr.PasskeyPublicKey = pubKeyPEM
	usr.PasskeySignCount = signCount

	// database client
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to get database client:", err)
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	err = usr.Save(dbClient)
	if err != nil {
		fmt.Println("❌ [postUserPasskey] failed to save user:", err)
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	fmt.Println("🔑 [postUserPasskey] 7 FINISHED 🟢")

	respondOK(w)
}

func postBlockUser(w http.ResponseWriter, r *http.Request) {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if usr == nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// parse request

	type BlockUserRequest struct {
		ID string `json:"id"`
	}

	var req BlockUserRequest
	jsonDecoder := json.NewDecoder(r.Body)
	jsonDecoder.DisallowUnknownFields()
	err = jsonDecoder.Decode(&req)
	if err != nil {
		fmt.Println("❌ [postBlockUser] failed to decode json request:", err)
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// check if the request is valid
	if req.ID == "" {
		fmt.Println("❌ [postBlockUser] invalid request")
		respondWithError(http.StatusBadRequest, "invalid request", w)
		return
	}

	// block the user
	usr.BlockUser(req.ID)

	// save the user
	err = usr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	blockedUsers := usr.GetBlockedUsers()
	respondWithJSON(http.StatusOK, blockedUsers, w)
}

func postUnblockUser(w http.ResponseWriter, r *http.Request) {
	dbClient, err := dbclient.SharedUserDB()
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	usr, err := getUserFromRequestContext(r)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}
	if usr == nil {
		respondWithError(http.StatusBadRequest, "", w)
		return
	}

	// parse request

	type UnblockUserRequest struct {
		ID string `json:"id"`
	}

	var req UnblockUserRequest
	jsonDecoder := json.NewDecoder(r.Body)
	jsonDecoder.DisallowUnknownFields()
	err = jsonDecoder.Decode(&req)
	if err != nil {
		fmt.Println("❌ [postUnblockUser] failed to decode json request:", err)
		respondWithError(http.StatusBadRequest, "can't decode json request", w)
		return
	}

	// check if the request is valid
	if req.ID == "" {
		fmt.Println("❌ [postUnblockUser] invalid request")
		respondWithError(http.StatusBadRequest, "invalid request", w)
		return
	}

	// unblock the user
	usr.UnblockUser(req.ID)

	// save the user
	err = usr.Save(dbClient)
	if err != nil {
		respondWithError(http.StatusInternalServerError, "", w)
		return
	}

	blockedUsers := usr.GetBlockedUsers()
	respondWithJSON(http.StatusOK, blockedUsers, w)
}

// ----------------------------------------------
// Utility functions
// ----------------------------------------------

func getSessionFieldPasskeyChallenge(r *http.Request) (string, error) {
	session, err := getSessionFromRequestContext(r)
	if err != nil {
		return "", err
	}
	challenge := session.Values[SESSION_FIELD_PASSKEY_CHALLENGE]
	// cast to string
	challengeStr, ok := challenge.(string)
	if !ok || challengeStr == "" {
		return "", fmt.Errorf("no challenge found in session")
	}
	return challengeStr, nil
}
