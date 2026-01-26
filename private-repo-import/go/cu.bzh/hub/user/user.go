package user

import (
	"crypto/sha1"
	"errors"
	"fmt"
	"math"
	"sort"
	"strings"
	"time"

	"cu.bzh/hub/dbclient"
	"cu.bzh/utils"
)

// Note: time.Time is encoded/decoded using RFC3339

var (
	reservedUsernames []string = []string{
		"noob",
		"n00b",
		"newbie", // current default username
		"guest",
		"anonymous",
		"anon",
		"visitor",
		"user",
		"player",
		"rookie",
	}
)

const DefaultUsername string = "newbie"

const (
	salt = "G00dm@n"
	// User DB storage
	USER_FIELD_BOOTS                             string = "av_boots"
	USER_FIELD_HAIR                              string = "av_hair"
	USER_FIELD_JACKET                            string = "av_jacket"
	USER_FIELD_PANTS                             string = "av_pants"
	USER_FIELD_SHIRT                             string = "av_shirt"
	USER_FIELD_EYESCOLOR                         string = "av_eyesColor"
	USER_FIELD_NOSECOLOR                         string = "av_noseColor"
	USER_FIELD_SKINCOLOR                         string = "av_skinColor"
	USER_FIELD_SKINCOLOR2                        string = "av_skinColor2"
	USER_FIELD_MOUTHCOLOR                        string = "av_mouthColor"
	USER_FIELD_SKIN_COLOR_IDX                    string = "av_skinColorIndex"
	USER_FIELD_EYES_COLOR_IDX                    string = "av_eyesColorIndex"
	USER_FIELD_EYES_IDX                          string = "av_eyesIndex"
	USER_FIELD_NOSE_IDX                          string = "av_noseIndex"
	USER_FIELD_DATEOFBIRTH                       string = "dob"
	USER_FIELD_ESTIMATED_DATEOFBIRTH             string = "estimatedDob"
	USER_FIELD_TAGS                              string = "tags"
	USER_FIELD_USERNAME                          string = "username"
	USER_FIELD_APPLE_PUSH_TOKENS                 string = "applePushTokens"
	USER_FIELD_FIREBASE_PUSH_TOKENS              string = "firebasePushTokens"
	USER_FIELD_APPLE_DEBUG_PUSH_TOKENS           string = "appleDebugPushTokens"
	USER_FIELD_FIREBASE_DEBUG_PUSH_TOKENS        string = "firebaseDebugPushTokens"
	USER_FIELD_BIO                               string = "bio"
	USER_FIELD_DISCORD                           string = "discord"
	USER_FIELD_X                                 string = "x"
	USER_FIELD_TIKTOK                            string = "tiktok"
	USER_FIELD_WEBSITE                           string = "website"
	USER_FIELD_GITHUB                            string = "github"
	USER_FIELD_LASTSEEN                          string = "lastseen"
	USER_FIELD_UPDATED                           string = "updated"
	USER_FIELD_CREATEDWITHCLIENTVERSION          string = "createdClientVersion"
	USER_FIELD_CREATEDWITHCLIENTBUILDNUMBER      string = "createdClientBuildNumber"
	USER_FIELD_PHONE                             string = "phone"
	USER_FIELD_PHONE_NEW                         string = "phoneNew"
	USER_FIELD_PHONE_VERIF_CODE                  string = "phoneVerifCode"
	USER_FIELD_PARENT_PHONE                      string = "parentPhone"
	USER_FIELD_PARENT_PHONE_NEW                  string = "parentPhoneNew"
	USER_FIELD_PARENT_PHONE_VERIF_CODE           string = "parentPhoneVerifCode"
	USER_FIELD_PARENTAL_CONTROL_APPROVED         string = "parentalControlApproved"
	USER_FIELD_PARENTAL_CONTROL_CHAT_ALLOWED     string = "parentalControlChatAllowed"
	USER_FIELD_PARENTAL_CONTROL_MESSAGES_ALLOWED string = "parentalControlMessagesAllowed"
	USER_FIELD_AUTO_FRIEND_REQUEST_COUNT         string = "autoFriendRequestCount"
	USER_FIELD_PASSKEY_AAGUID                    string = "passkeyAAGUID"
	USER_FIELD_PASSKEY_CREDENTIAL_ID             string = "passkeyCredentialID"
	USER_FIELD_PASSKEY_PUBLIC_KEY                string = "passkeyPublicKey"
	USER_FIELD_PASSKEY_SIGN_COUNT                string = "passkeySignCount"
	USER_FIELD_BLOCKED_USERS                     string = "blockedUsers"
)

type Color struct {
	R uint8 `bson:"r" json:"r"`
	G uint8 `bson:"g" json:"g"`
	B uint8 `bson:"b" json:"b"`
}

// Push token (APNS or FCM)
type Token struct {
	Token     string    `bson:"tk" json:"tk"`
	Variant   string    `bson:"variant" json:"variant"`
	Timestamp time.Time `bson:"tm" json:"tm"`
}

func NewToken(token, variant string, timestamp time.Time) Token {
	return Token{
		Token:     token,
		Variant:   variant,
		Timestamp: timestamp,
	}
}

// User constains all user fields
type User struct {
	//
	ID string `bson:"id" json:"id"`
	// Username is the name displayed and used for login
	Username string `bson:"username,omitempty" json:"username,omitempty"`

	// IsTestAccount is true if the account is a test account.
	// Test accounts are used for testing purposes, some checks and safety features are disabled for them.
	// A user becomes a test account as soon as the secret phone number is provided for it,
	// it still has to be verified with secret code to become a verified test account
	IsTestAccount         bool `bson:"isTestAccount,omitempty" json:"isTestAccount,omitempty"`
	IsVerifiedTestAccount bool `bson:"isVerifiedTestAccount,omitempty" json:"isVerifiedTestAccount,omitempty"`

	// Assigned in order of creation (manually, using CLI for now)
	Number int `bson:"number,omitempty" json:"number,omitempty"`

	// Email
	Email          string  `bson:"email,omitempty" json:"email,omitempty"`
	EmailTemporary *string `bson:"emailTmp,omitempty" json:"emailTmp,omitempty"`
	EmailVerifCode *string `bson:"email-confirm-hash,omitempty" json:"email-confirm-hash,omitempty"`
	// Idea for later: keep track of email history
	// EmailHistory []string `bson:"email-history,omitempty" json:"email-history,omitempty"`

	// ParentEmail
	ParentEmail          string  `bson:"parentEmail,omitempty" json:"parentEmail,omitempty"`
	ParentEmailTemporary *string `bson:"parentEmailTmp,omitempty" json:"parentEmailTmp,omitempty"`
	ParentEmailVerifCode *string `bson:"parentEmailVerifCode,omitempty" json:"parentEmailVerifCode,omitempty"`

	// Phone contains the (confirmed) phone number of the user.
	Phone *string `bson:"phone,omitempty" json:"phone,omitempty"`
	// PhoneNew contains the new phone number that the user wants to set. (not yet confirmed)
	PhoneNew *string `bson:"phoneNew,omitempty" json:"phoneNew,omitempty"`
	// PhoneVerifCode
	PhoneVerifCode *string `bson:"phoneVerifCode,omitempty" json:"phoneVerifCode,omitempty"`

	// ParentPhone contains the (confirmed) phone number of the user's parent.
	ParentPhone *string `bson:"parentPhone,omitempty" json:"parentPhone,omitempty"`
	// ParentPhoneNew contains the new parent's phone number that the user wants to set. (not yet confirmed)
	ParentPhoneNew *string `bson:"parentPhoneNew,omitempty" json:"parentPhoneNew,omitempty"`
	// ParentPhoneVerifCode
	ParentPhoneVerifCode *string `bson:"parentPhoneVerifCode,omitempty" json:"parentPhoneVerifCode,omitempty"`

	// Use SetPassword to set it
	Password  string     `bson:"password,omitempty" json:"password,omitempty"`
	FirstName string     `bson:"firstname,omitempty" json:"firstname,omitempty"`
	LastName  string     `bson:"lastname,omitempty" json:"lastname,omitempty"`
	Created   *time.Time `bson:"created" json:"created"`
	Updated   *time.Time `bson:"updated" json:"updated"`
	LastSeen  *time.Time `bson:"lastseen" json:"lastseen"`
	Dob       *time.Time `bson:"dob,omitempty" json:"dob,omitempty"`
	// computed when age is provided -> current date minus age
	EstimatedDob *time.Time `bson:"estimatedDob,omitempty" json:"estimatedDob,omitempty"`

	// Assigned in order of creation (manually, using CLI for now)
	SteamKey string `bson:"steamkey,omitempty" json:"steamkey,omitempty"`

	// Token used along with ID to authenticate user.
	// Created when creating the account or login in.
	// Shared accross devices when connecting from more than one.
	// Can be revoked to log out from all devices.
	Token string `bson:"token,omitempty" json:"token,omitempty"`

	// Magic key is used to login with magic link, generated when asking for magic link
	MagicKey string `bson:"magickey,omitempty" json:"magickey,omitempty"`

	// MagicKeyExpirationDate is the date when the magic key expires
	MagicKeyExpirationDate *time.Time `bson:"magickey-exp-date,omitempty" json:"magickey-exp-date,omitempty"`

	// Flag indicating if the user account is considered deleted
	Deleted bool `bson:"deleted,omitempty" json:"deleted,omitempty"`

	// Delete reason is the reason for the deletion
	// 0: user deleted their own account
	// 1: admin deleted the account (ban)
	DeleteReason int `bson:"deleteReason,omitempty" json:"deleteReason,omitempty"`

	// IsNew means "not saved in database"
	IsNew bool `bson:"-" json:"-"`

	// Fields not stored in database, but only for computing usage stats
	IsDeveloper bool `bson:"-" json:"-"`
	IsArtist    bool `bson:"-" json:"-"`

	// names of items used for avatar body parts (of the form "<repo>.<name>")
	AvatarHead  string `bson:"head,omitempty" json:"head,omitempty"`
	AvatarBody  string `bson:"body,omitempty" json:"body,omitempty"`
	AvatarLArm  string `bson:"leftArm,omitempty" json:"leftArm,omitempty"`
	AvatarRArm  string `bson:"rightArm,omitempty" json:"rightArm,omitempty"`
	AvatarLHand string `bson:"leftHand,omitempty" json:"leftHand,omitempty"`
	AvatarRHand string `bson:"rightHand,omitempty" json:"rightHand,omitempty"`
	AvatarLLeg  string `bson:"leftLeg,omitempty" json:"leftLeg,omitempty"`
	AvatarRLeg  string `bson:"rightLeg,omitempty" json:"rightLeg,omitempty"`
	AvatarLFoot string `bson:"leftFoot,omitempty" json:"leftFoot,omitempty"`
	AvatarRFoot string `bson:"rightFoot,omitempty" json:"rightFoot,omitempty"`

	// names of items used for equipement (of the form "<repo>.<name>")
	AvatarTop     string `bson:"top,omitempty" json:"top,omitempty"`
	AvatarLSleeve string `bson:"leftSleeve,omitempty" json:"leftSleeve,omitempty"`
	AvatarRSleeve string `bson:"rightSleeve,omitempty" json:"rightSleeve,omitempty"`
	AvatarLGlove  string `bson:"leftGlove,omitempty" json:"leftGlove,omitempty"`
	AvatarRGlove  string `bson:"rightGlove,omitempty" json:"rightGlove,omitempty"`
	AvatarLPant   string `bson:"leftPant,omitempty" json:"leftPant,omitempty"`
	AvatarRPant   string `bson:"rightPant,omitempty" json:"rightPant,omitempty"`
	AvatarLBoot   string `bson:"leftBoot,omitempty" json:"leftBoot,omitempty"`
	AvatarRBoot   string `bson:"rightBoot,omitempty" json:"rightBoot,omitempty"`

	// fields for avatar v3
	AvatarHair       string `bson:"av_hair,omitempty" json:"av_hair,omitempty"`
	AvatarJacket     string `bson:"av_jacket,omitempty" json:"av_jacket,omitempty"`
	AvatarPants      string `bson:"av_pants,omitempty" json:"av_pants,omitempty"`
	AvatarBoots      string `bson:"av_boots,omitempty" json:"av_boots,omitempty"`
	AvatarEyesColor  *Color `bson:"av_eyesColor,omitempty" json:"av_eyesColor,omitempty"`
	AvatarNoseColor  *Color `bson:"av_noseColor,omitempty" json:"av_noseColor,omitempty"`
	AvatarSkinColor  *Color `bson:"av_skinColor,omitempty" json:"av_skinColor,omitempty"`
	AvatarSkinColor2 *Color `bson:"av_skinColor2,omitempty" json:"av_skinColor2,omitempty"`
	AvatarMouthColor *Color `bson:"av_mouthColor,omitempty" json:"av_mouthColor,omitempty"`
	// not used?
	AvatarShirt string `bson:"av_shirt,omitempty" json:"av_shirt,omitempty"`

	// fields for avatar v4
	// Index value of 0 means "no value"
	AvatarSkinColorIndex int `bson:"av_skinColorIndex,omitempty" json:"av_skinColorIndex,omitempty"`
	AvatarEyesColorIndex int `bson:"av_eyesColorIndex,omitempty" json:"av_eyesColorIndex,omitempty"`
	AvatarEyesIndex      int `bson:"av_eyesIndex,omitempty" json:"av_eyesIndex,omitempty"`
	AvatarNoseIndex      int `bson:"av_noseIndex,omitempty" json:"av_noseIndex,omitempty"`

	// tags
	Tags map[string]bool `bson:"tags,omitempty" json:"tags,omitempty"`

	// Push notification tokens
	ApplePushTokens         []Token `bson:"applePushTokens,omitempty" json:"applePushTokens,omitempty"`
	AppleDebugPushTokens    []Token `bson:"appleDebugPushTokens,omitempty" json:"appleDebugPushTokens,omitempty"`
	FirebasePushTokens      []Token `bson:"firebasePushTokens,omitempty" json:"firebasePushTokens,omitempty"`
	FirebaseDebugPushTokens []Token `bson:"firebaseDebugPushTokens,omitempty" json:"firebaseDebugPushTokens,omitempty"`

	// bio and other profile fields
	Bio     string `bson:"bio,omitempty" json:"bio,omitempty"`
	Discord string `bson:"discord,omitempty" json:"discord,omitempty"`
	X       string `bson:"x,omitempty" json:"x,omitempty"`
	Tiktok  string `bson:"tiktok,omitempty" json:"tiktok,omitempty"`
	Website string `bson:"website,omitempty" json:"website,omitempty"`
	Github  string `bson:"github,omitempty" json:"github,omitempty"`

	// version of the client used to create the user account
	CreatedWithClientVersion     string `bson:"createdClientVersion,omitempty" json:"createdClientVersion,omitempty"`
	CreatedWithClientBuildNumber *int   `bson:"createdClientBuildNumber,omitempty" json:"createdClientBuildNumber,omitempty"`

	// Parental control
	ParentalControlApproved        *bool `bson:"parentalControlApproved,omitempty" json:"parentalControlApproved,omitempty"`
	ParentalControlChatAllowed     *bool `bson:"parentalControlChatAllowed,omitempty" json:"parentalControlChatAllowed,omitempty"`
	ParentalControlMessagesAllowed *bool `bson:"parentalControlMessagesAllowed,omitempty" json:"parentalControlMessagesAllowed,omitempty"`
	// This will be added later for premium child accounts
	// ParentalControlAIModeration	bool `bson:"parentalControlAIModeration,omitempty" json:"parentalControlAIModeration,omitempty"`

	AutoFriendRequestCount int `bson:"autoFriendRequestCount,omitempty" json:"autoFriendRequestCount,omitempty"`

	// Passkey
	// ------------------------------------------
	//
	PasskeyAAGUID string `bson:"passkeyAAGUID,omitempty" json:"passkeyAAGUID,omitempty"`
	//
	PasskeyCredentialID string `bson:"passkeyCredentialID,omitempty" json:"passkeyCredentialID,omitempty"`
	// base64 representation of PEM public key
	PasskeyPublicKey string `bson:"passkeyPublicKey,omitempty" json:"passkeyPublicKey,omitempty"`
	// sign count is a counter of private key uses. It is used to prevent replay attacks.
	PasskeySignCount int `bson:"passkeySignCount,omitempty" json:"passkeySignCount,omitempty"`

	// blocked users
	BlockedUsers []string `bson:"blockedUsers,omitempty" json:"blockedUsers,omitempty"`
}

func (u *User) ComputeExactOrEstimatedAge() (int, error) {
	var doBToUse *time.Time = nil
	if u.Dob != nil {
		doBToUse = u.Dob
	} else if u.EstimatedDob != nil {
		doBToUse = u.EstimatedDob
	} else {
		return 0, errors.New("No date of birth or estimated date of birth")
	}
	// compute interval between now and dob
	// then convert it to years
	// then return it
	age := time.Now().Sub(*doBToUse)
	ageInYears := age.Hours() / 24 / 365.25
	years := int(math.Floor(ageInYears))
	return years, nil
}

// SetPassword ...
func (u *User) SetPassword(s string) {
	data := []byte(s + salt)
	u.Password = fmt.Sprintf("%x", sha1.Sum(data))
}

// CheckPassword ...
func (u *User) CheckPassword(s string) bool {
	data := []byte(s + salt)
	received := fmt.Sprintf("%x", sha1.Sum(data))
	return received == u.Password
}

// CheckPassword ...
func (u *User) CheckMagicKey(s string) bool {
	if !u.IsMagicKeyValid() {
		return false
	}
	return s == u.MagicKey
}

// IsMagicKeyValid returns true if the user has a non-expired magic key
func (u *User) IsMagicKeyValid() bool {

	if u.MagicKey == "" {
		return false
	}

	if u.MagicKeyExpirationDate == nil {
		return false
	}

	if u.MagicKeyExpirationDate.Before(time.Now()) {
		return false
	}

	return true
}

func GenerateNumericalCodeWithExpirationDate(length int, validity time.Duration) (code string, expirationDate time.Time) {
	code = GenerateRandomNumericalKey(length)
	now := time.Now()
	expirationDate = now.Add(validity)
	return
}

// GenerateMagicLink ...
func (u *User) UpdateEmailAddress(dbClient *dbclient.Client) error {
	now := time.Now()
	expiration := now.Add(time.Minute * 30)

	// TODO: [gaetan] maybe we should not update the u object until the DB update has succeeded
	u.MagicKey = GenerateRandomNumericalKey(6)
	u.Updated = &now
	u.MagicKeyExpirationDate = &expiration

	updateData := map[string]interface{}{
		"magickey":          u.MagicKey,
		"magickey-exp-date": u.MagicKeyExpirationDate,
		USER_FIELD_UPDATED:  now,
		USER_FIELD_LASTSEEN: now,
	}

	filter := map[string]interface{}{"id": u.ID}
	err := dbClient.UpdateRecord(dbclient.UserCollectionName, filter, updateData)
	if err == nil {
		// DB update succeeded, let's update the user object
		u.Updated = &now
		u.LastSeen = &now
	}

	return err
}

func (usr *User) GetEmailAddressToUse() string {
	if usr.Email != "" {
		return usr.Email
	}
	if usr.ParentEmail != "" {
		return usr.ParentEmail
	}
	return ""
}

func (usr *User) GetPhoneNumberToUse() (string, error) {
	if usr.Phone != nil && *usr.Phone != "" {
		return *usr.Phone, nil
	}
	if usr.ParentPhone != nil && *usr.ParentPhone != "" {
		return *usr.ParentPhone, nil
	}
	return "", errors.New("no phone number found")
}

// IsBlocked returns true if the user is blocked by the given userID
func (usr *User) IsBlocked(userID string) bool {
	if usr.BlockedUsers == nil {
		return false
	}
	return containsString(usr.BlockedUsers, userID)
}

// BlockUser blocks the given userID
func (usr *User) BlockUser(userID string) {
	if usr.BlockedUsers == nil {
		usr.BlockedUsers = make([]string, 0)
	} else if usr.IsBlocked(userID) {
		return
	}
	usr.BlockedUsers = append(usr.BlockedUsers, userID)
}

// UnblockUser unblocks the given userID
func (usr *User) UnblockUser(userID string) {
	if usr.BlockedUsers == nil {
		return
	}
	for i, blockedUserID := range usr.BlockedUsers {
		if blockedUserID == userID {
			usr.BlockedUsers = append(usr.BlockedUsers[:i], usr.BlockedUsers[i+1:]...)
			return
		}
	}
}

// GetBlockedUsers returns the list of blocked users
func (usr *User) GetBlockedUsers() []string {
	if usr.BlockedUsers == nil {
		return make([]string, 0)
	}
	return usr.BlockedUsers
}

// New creates a new user, with empty ID
func New() (*User, error) {
	now := time.Now()
	userID, err := utils.UUID()
	if err != nil {
		return nil, err
	}
	token, err := utils.UUID()
	if err != nil {
		return nil, err
	}
	return &User{
			ID:       userID,
			Token:    token,
			Created:  &now,
			Updated:  &now,
			LastSeen: &now,
			IsNew:    true},
		nil
}

// GetAll returns all users stored in the database
func GetAll(dbClient *dbclient.Client) ([]*User, error) {
	users := make([]*User, 0)
	err := dbClient.GetAllRecords(dbclient.UserCollectionName, &users)
	if err != nil {
		return nil, err
	}

	// filter out deleted users
	// TODO: [gaetan] maybe we should do this filtering in the DB query
	filtered := make([]*User, 0)
	for _, u := range users {
		if !u.Deleted {
			filtered = append(filtered, u)
		}
	}

	return filtered, err
}

func CountAll(dbClient *dbclient.Client) (int64, error) {
	return dbClient.CountRecords(dbclient.UserCollectionName)
}

func CountUsersBornAfter(dbClient *dbclient.Client, date time.Time) (int64, error) {
	filter := map[string]interface{}{"dob": map[string]interface{}{"$gt": date}}
	return dbClient.CountRecordsMatchingFilter(dbclient.UserCollectionName, filter)
}

func CountUsers(dbClient *dbclient.Client, filter map[string]interface{}) (int64, error) {
	return dbClient.CountRecordsMatchingFilter(dbclient.UserCollectionName, filter)
}

// IsUsernameAvailable returns true if there is no user having the given username, false otherwise
func IsUsernameAvailable(dbClient *dbclient.Client, username string) (bool, error) {
	if containsString(reservedUsernames, username) {
		return false, nil
	}
	count, err := dbClient.CountRecordsMatchingKeyValue(dbclient.UserCollectionName, USER_FIELD_USERNAME, username)
	return count == 0, err
}

// GetByUsername returns user with given username
// if not found, returns no error and nil user
func GetByUsername(dbClient *dbclient.Client, username string) (*User, error) {
	if containsString(reservedUsernames, username) {
		return nil, nil
	}
	user := &User{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.UserCollectionName, USER_FIELD_USERNAME, username, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}
	return user, err
}

// func GetByCreationDateSince(dbClient *dbclient.Client, date time.Time) ([]*User, error) {
// 	if dbClient == nil {
// 		return nil, errors.New("dbClient is nil")
// 	}
// 	if date.After(time.Now()) {
// 		return nil, errors.New("date is in the future")
// 	}
// 	users := make([]*User, 0)
// 	filter := map[string]interface{}{
// 		"created": map[string]time.Time{
// 			"$gte": date,
// 		},
// 	}
// 	var findOpts *dbclient.FindOptions = nil
// 	err := dbClient.GetRecordsMatchingFilter(dbclient.UserCollectionName, filter, findOpts, &users)
// 	if err != nil {
// 		return nil, err
// 	}
// 	return users, nil
// }

func GetByCreationDateBetween(dbClient *dbclient.Client, dateBegin, dateEnd time.Time) ([]*User, error) {
	if dbClient == nil {
		return nil, errors.New("dbClient is nil")
	}
	if dateBegin.After(dateEnd) {
		return nil, errors.New("dateBegin is after dateEnd")
	}

	users := make([]*User, 0)

	filter := map[string]interface{}{
		"created": map[string]time.Time{
			"$gt": dateBegin,
			"$lt": dateEnd,
		},
	}

	var findOpts *dbclient.FindOptions = nil
	err := dbClient.GetRecordsMatchingFilter(dbclient.UserCollectionName, filter, findOpts, &users)
	if err != nil {
		return nil, err
	}

	return users, nil
}

// GetByEmail returns user with given email
// if not found, returns no error and nil user
func GetByEmail(dbClient *dbclient.Client, email string) (*User, error) {
	user := &User{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.UserCollectionName, "email", email, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}
	return user, err
}

func GetUsernameForUsers(dbClient *dbclient.Client, userIDs []string) (usernames []string, err error) {
	users := make([]*User, 0)
	err = dbClient.GetRecordsWithFieldHavingOneOfValues(dbclient.UserCollectionName, "id", userIDs, &users)
	if err != nil {
		return
	}
	usernames = make([]string, len(users))
	for i, u := range users {
		usernames[i] = u.Username
	}
	return
}

// GetByEmail returns user with given email
// if not found, returns no error and nil user
func GetByParentEmail(dbClient *dbclient.Client, parentEmail string) (*User, error) {
	user := &User{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.UserCollectionName, "parentEmail", parentEmail, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}
	return user, err
}

// GetByEmailConfirmHash returns user with given email confirm hash
// if not found, returns no error and nil user
func GetByEmailConfirmHash(dbClient *dbclient.Client, hash string) (*User, error) {
	user := &User{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.UserCollectionName, "email-confirm-hash", hash, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}
	return user, err
}

// GetByID returns user with given ID.
// If user is not found, returns no error and nil user.
func GetByID(dbClient *dbclient.Client, userID string) (*User, error) {
	filter := make(map[string]interface{})
	filter["id"] = userID

	user := &User{}
	found, err := dbClient.GetSingleRecord(dbclient.UserCollectionName, filter, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}

	return user, err
}

// GetWithIDAndToken gets user by ID
// returns nil user with no error if not found or if token is not ok
// err != nil only if there's an internal server issue
func GetWithIDAndToken(dbClient *dbclient.Client, userID, token string) (*User, error) {

	// Find user by ID
	usr, err := GetByID(dbClient, userID)
	if err != nil {
		return nil, err
	}

	// Return if user is not found or flagged as deleted
	if usr == nil || usr.Deleted {
		return nil, nil
	}

	// Return is token doesn't match
	if usr.Token != token {
		return nil, nil
	}

	// Do not keep tokens in user object
	// usr.Token = ""

	return usr, nil // success
}

// GetByPhone gets user by phone number
// users := make([]User, 0)
// err := dbClient.GetRecordsMatchingKeyValue(dbclient.UserCollectionName, "phone", phone, users)
func GetByPhone(dbClient *dbclient.Client, phone string) (*User, error) {
	user := &User{}
	found, err := dbClient.GetSingleRecordMatchingKeyValue(dbclient.UserCollectionName, "phone", phone, user)
	if err != nil || !found || user.Deleted {
		user = nil
	}
	return user, err
}

// List lists users
func List(dbClient *dbclient.Client, filterStr string) ([]*User, error) {
	var results []*User

	findOptions := &dbclient.FindOptions{}
	// default limit
	findOptions.SetLimit(100)
	// default sort
	sort := make(map[string]interface{})
	sort["updated"] = -1
	findOptions.SetSort(sort)

	filter := make(map[string]interface{})
	if len(filterStr) > 0 {
		// regex option is better in that situation
		// "rock" search will return "rock", "rock123", "big_rock"
		// with $text index, words have to match exactly.
		// filter = append(filter, bson.E{"$text", bson.M{"$search": f.String}})
		filter[USER_FIELD_USERNAME] = dbclient.Regex{Pattern: ".*" + filterStr + ".*", Options: "i"}
	}

	err := dbClient.GetRecordsMatchingFilter(dbclient.UserCollectionName, filter, findOptions, &results)
	if err != nil {
		return nil, err
	}

	// filter out deleted users
	filtered := make([]*User, 0)
	for _, u := range results {
		if !u.Deleted {
			filtered = append(filtered, u)
		}
	}

	return filtered, err
}

// Save writes the user in database, in given collection
func (u *User) Save(dbClient *dbclient.Client) error {

	u.Email = SanitizeEmail(u.Email)

	if !u.IsNew {
		// UPDATE
		err := u.update(dbClient)
		if err != nil {
			return err
		}
	} else {
		// INSERT
		err := u.insert(dbClient)
		if err != nil {
			return err
		}
		u.IsNew = false
	}
	return nil
}

func (u *User) insert(dbClient *dbclient.Client) error {
	return dbClient.InsertNewRecord(dbclient.UserCollectionName, u)
}

func (u *User) UpdateLastSeen(dbClient *dbclient.Client) error {
	now := time.Now()

	updateData := map[string]interface{}{
		USER_FIELD_LASTSEEN: now,
	}

	filter := map[string]interface{}{"id": u.ID}
	err := dbClient.UpdateRecord(dbclient.UserCollectionName, filter, updateData)
	if err == nil {
		// DB update succeeded, let's update the user object
		u.LastSeen = &now
	}

	return err
}

func (u *User) update(dbClient *dbclient.Client) error {

	// construct a map containing the data to update
	updateData := make(map[string]interface{})

	if u.Username != "" {
		if containsString(reservedUsernames, u.Username) {
			return errors.New(u.Username + " is a reserved username")
		}
		updateData[USER_FIELD_USERNAME] = u.Username
	}

	updateData["isTestAccount"] = u.IsTestAccount
	updateData["isVerifiedTestAccount"] = u.IsVerifiedTestAccount

	if u.Dob != nil {
		updateData[USER_FIELD_DATEOFBIRTH] = u.Dob
	}

	if u.EstimatedDob != nil {
		updateData[USER_FIELD_ESTIMATED_DATEOFBIRTH] = u.EstimatedDob
	}

	if u.Token != "" {
		updateData["token"] = u.Token
	}

	if u.Email != "" {
		updateData["email"] = u.Email
	}

	if u.EmailTemporary != nil {
		updateData["emailTmp"] = *u.EmailTemporary
	}

	if u.EmailVerifCode != nil {
		updateData["email-confirm-hash"] = *u.EmailVerifCode
	}

	if u.ParentEmail != "" {
		updateData["parentEmail"] = u.ParentEmail
	}

	if u.ParentEmailTemporary != nil {
		updateData["parentEmailTmp"] = *u.ParentEmailTemporary
	}

	if u.ParentEmailVerifCode != nil {
		updateData["parentEmailVerifCode"] = *u.ParentEmailVerifCode
	}

	if u.Password != "" {
		updateData["password"] = u.Password
	}

	if u.FirstName != "" {
		updateData["firstname"] = u.FirstName
	}

	if u.LastName != "" {
		updateData["lastname"] = u.LastName
	}

	if u.Number != 0 {
		updateData["number"] = u.Number
	}

	if u.SteamKey != "" {
		updateData["steamkey"] = u.SteamKey
	}

	if u.Deleted {
		updateData["deleted"] = u.Deleted
	}

	if u.DeleteReason != 0 {
		updateData["deleteReason"] = u.DeleteReason
	}

	// TOOD: gdevillele: check if we can get the serialialization field name (https://pkg.go.dev/reflect#StructTag)

	// body parts
	if u.AvatarHead != "" {
		updateData["head"] = u.AvatarHead
	}
	if u.AvatarBody != "" {
		updateData["body"] = u.AvatarBody
	}
	if u.AvatarLArm != "" {
		updateData["leftArm"] = u.AvatarLArm
	}
	if u.AvatarRArm != "" {
		updateData["rightArm"] = u.AvatarRArm
	}
	if u.AvatarLHand != "" {
		updateData["leftHand"] = u.AvatarLHand
	}
	if u.AvatarRHand != "" {
		updateData["rightHand"] = u.AvatarRHand
	}
	if u.AvatarLLeg != "" {
		updateData["leftLeg"] = u.AvatarLLeg
	}
	if u.AvatarRLeg != "" {
		updateData["rightLeg"] = u.AvatarRLeg
	}
	if u.AvatarLFoot != "" {
		updateData["leftFoot"] = u.AvatarLFoot
	}
	if u.AvatarRFoot != "" {
		updateData["rightFoot"] = u.AvatarRFoot
	}

	// equipment
	if u.AvatarTop != "" {
		updateData["top"] = u.AvatarTop
	}
	if u.AvatarLSleeve != "" {
		updateData["leftSleeve"] = u.AvatarLSleeve
	}
	if u.AvatarRSleeve != "" {
		updateData["rightSleeve"] = u.AvatarRSleeve
	}
	if u.AvatarLGlove != "" {
		updateData["leftGlove"] = u.AvatarLGlove
	}
	if u.AvatarRGlove != "" {
		updateData["rightGlove"] = u.AvatarRGlove
	}
	if u.AvatarLPant != "" {
		updateData["leftPant"] = u.AvatarLPant
	}
	if u.AvatarRPant != "" {
		updateData["rightPant"] = u.AvatarRPant
	}
	if u.AvatarLBoot != "" {
		updateData["leftBoot"] = u.AvatarLBoot
	}
	if u.AvatarRBoot != "" {
		updateData["rightBoot"] = u.AvatarRBoot
	}
	// v3
	if u.AvatarBoots != "" {
		updateData[USER_FIELD_BOOTS] = u.AvatarBoots
	}
	if u.AvatarHair != "" {
		updateData[USER_FIELD_HAIR] = u.AvatarHair
	}
	if u.AvatarJacket != "" {
		updateData[USER_FIELD_JACKET] = u.AvatarJacket
	}
	if u.AvatarPants != "" {
		updateData[USER_FIELD_PANTS] = u.AvatarPants
	}
	if u.AvatarShirt != "" {
		updateData[USER_FIELD_SHIRT] = u.AvatarShirt
	}
	if u.AvatarEyesColor != nil {
		updateData[USER_FIELD_EYESCOLOR] = u.AvatarEyesColor
	}
	if u.AvatarNoseColor != nil {
		updateData[USER_FIELD_NOSECOLOR] = u.AvatarNoseColor
	}
	if u.AvatarSkinColor != nil {
		updateData[USER_FIELD_SKINCOLOR] = u.AvatarSkinColor
	}
	if u.AvatarSkinColor2 != nil {
		updateData[USER_FIELD_SKINCOLOR2] = u.AvatarSkinColor2
	}
	if u.AvatarMouthColor != nil {
		updateData[USER_FIELD_MOUTHCOLOR] = u.AvatarMouthColor
	}
	// avatar v4 fields
	if u.AvatarSkinColorIndex != 0 {
		updateData[USER_FIELD_SKIN_COLOR_IDX] = u.AvatarSkinColorIndex
	}
	if u.AvatarEyesColorIndex != 0 {
		updateData[USER_FIELD_EYES_COLOR_IDX] = u.AvatarEyesColorIndex
	}
	if u.AvatarEyesIndex != 0 {
		updateData[USER_FIELD_EYES_IDX] = u.AvatarEyesIndex
	}
	if u.AvatarNoseIndex != 0 {
		updateData[USER_FIELD_NOSE_IDX] = u.AvatarNoseIndex
	}

	if u.Tags != nil {
		updateData[USER_FIELD_TAGS] = u.Tags
	}

	if u.ApplePushTokens != nil {
		updateData[USER_FIELD_APPLE_PUSH_TOKENS] = u.ApplePushTokens
	}
	if u.FirebasePushTokens != nil {
		updateData[USER_FIELD_FIREBASE_PUSH_TOKENS] = u.FirebasePushTokens
	}

	if u.AppleDebugPushTokens != nil {
		updateData[USER_FIELD_APPLE_DEBUG_PUSH_TOKENS] = u.AppleDebugPushTokens
	}
	if u.FirebaseDebugPushTokens != nil {
		updateData[USER_FIELD_FIREBASE_DEBUG_PUSH_TOKENS] = u.FirebaseDebugPushTokens
	}

	if u.CreatedWithClientVersion != "" {
		updateData[USER_FIELD_CREATEDWITHCLIENTVERSION] = u.CreatedWithClientVersion
	}
	if u.CreatedWithClientBuildNumber != nil {
		updateData[USER_FIELD_CREATEDWITHCLIENTBUILDNUMBER] = u.CreatedWithClientBuildNumber
	}

	// Bio / Social networks
	updateData[USER_FIELD_BIO] = u.Bio
	updateData[USER_FIELD_DISCORD] = u.Discord
	updateData[USER_FIELD_X] = u.X
	updateData[USER_FIELD_TIKTOK] = u.Tiktok
	updateData[USER_FIELD_WEBSITE] = u.Website
	updateData[USER_FIELD_GITHUB] = u.Github

	// TODO: use reflect to get the bson annotation for the fields (.Phone for instance)
	//       and use the annotation name to update the `updateData` dictionnary

	// Phone number
	if u.Phone != nil {
		updateData[USER_FIELD_PHONE] = u.Phone
	}
	if u.PhoneNew != nil {
		updateData[USER_FIELD_PHONE_NEW] = u.PhoneNew
	}
	if u.PhoneVerifCode != nil {
		updateData[USER_FIELD_PHONE_VERIF_CODE] = u.PhoneVerifCode
	}

	// Parent phone number
	if u.ParentPhone != nil {
		updateData[USER_FIELD_PARENT_PHONE] = u.ParentPhone
	}
	if u.ParentPhoneNew != nil {
		updateData[USER_FIELD_PARENT_PHONE_NEW] = u.ParentPhoneNew
	}
	if u.ParentPhoneVerifCode != nil {
		updateData[USER_FIELD_PARENT_PHONE_VERIF_CODE] = u.ParentPhoneVerifCode
	}

	// Parental Control
	if u.ParentalControlApproved != nil {
		updateData[USER_FIELD_PARENTAL_CONTROL_APPROVED] = u.ParentalControlApproved
	}
	if u.ParentalControlChatAllowed != nil {
		updateData[USER_FIELD_PARENTAL_CONTROL_CHAT_ALLOWED] = u.ParentalControlChatAllowed
	}
	if u.ParentalControlMessagesAllowed != nil {
		updateData[USER_FIELD_PARENTAL_CONTROL_MESSAGES_ALLOWED] = u.ParentalControlMessagesAllowed
	}

	if u.AutoFriendRequestCount != 0 {
		updateData[USER_FIELD_AUTO_FRIEND_REQUEST_COUNT] = u.AutoFriendRequestCount
	}

	// Passkey
	if u.PasskeyAAGUID != "" {
		updateData[USER_FIELD_PASSKEY_AAGUID] = u.PasskeyAAGUID
	}
	if u.PasskeyCredentialID != "" {
		updateData[USER_FIELD_PASSKEY_CREDENTIAL_ID] = u.PasskeyCredentialID
	}
	if u.PasskeyPublicKey != "" {
		updateData[USER_FIELD_PASSKEY_PUBLIC_KEY] = u.PasskeyPublicKey
	}
	if u.PasskeySignCount != 0 {
		updateData[USER_FIELD_PASSKEY_SIGN_COUNT] = u.PasskeySignCount
	}

	// blocked users
	if u.BlockedUsers != nil {
		updateData[USER_FIELD_BLOCKED_USERS] = u.BlockedUsers
	}

	now := time.Now()

	updateData[USER_FIELD_UPDATED] = now
	updateData[USER_FIELD_LASTSEEN] = now

	filter := map[string]interface{}{"id": u.ID}
	err := dbClient.UpdateRecord(dbclient.UserCollectionName, filter, updateData)
	if err == nil {
		// DB update succeeded, let's update the user object
		u.Updated = &now
		u.LastSeen = &now
	}

	return err
}

// GenerateMagicLink ...
func (u *User) GenerateMagicLink(dbClient *dbclient.Client) error {
	// TODO: [gaetan] maybe we should not update the u object until the DB update has succeeded
	u.MagicKey = GenerateRandomNumericalKey(6)
	err := u.UpdateAndSaveMagicKey(dbClient, time.Minute*30)
	return err
}

// u.MagicKey must be set prior to calling this function
func (u *User) UpdateAndSaveMagicKey(dbClient *dbclient.Client, expirationDuration time.Duration) error {
	now := time.Now()
	expiration := now.Add(expirationDuration)

	u.MagicKeyExpirationDate = &expiration

	updateData := map[string]interface{}{
		"magickey":          u.MagicKey,
		"magickey-exp-date": u.MagicKeyExpirationDate,
		USER_FIELD_UPDATED:  now,
		USER_FIELD_LASTSEEN: now,
	}

	filter := map[string]interface{}{"id": u.ID}
	err := dbClient.UpdateRecord(dbclient.UserCollectionName, filter, updateData)
	if err == nil {
		// DB update succeeded, let's update the user object
		u.Updated = &now
		u.LastSeen = &now
	}

	return err
}

// Remove the user from the database
// It is used when a user deletes their own account.
// (but not when an admin is banning a user)
func (u *User) Remove(dbClient *dbclient.Client) error {
	filter := map[string]interface{}{
		"id": u.ID,
	}
	found, err := dbClient.DeleteSingleRecordMatchingFilter(dbclient.UserCollectionName, filter)
	if err != nil {
		return err
	}
	if !found {
		return errors.New("user not found")
	}
	return nil
}

type DeleteReason int

const (
	DeleteReasonUserDeletedOwnAccount DeleteReason = 0
	DeleteReasonAdminDeletedAccount   DeleteReason = 1
)

func (u *User) Ban(dbClient *dbclient.Client, reason DeleteReason) error {
	u.Deleted = true
	u.DeleteReason = int(reason)
	err := u.Save(dbClient)
	if err != nil {
		return err
	}

	return err
}

// GetChosenBodyPartName ...
func (u *User) GetChosenBodyPartName(name string) (result string, err error) {
	if u == nil {
		err = errors.New("user is nil")
		fmt.Println("[GetChosenBodyPartName] ERROR:", err.Error())
		return
	}

	err = nil
	result = ""

	// // make sure `name` is the name of a body part, or return an error
	// if IsItemNameForBodyPart(name) == false {
	// 	return "", errors.New("provided name is not a body part name")
	// }

	switch name {
	case "head":
		result = u.AvatarHead
	case "body":
		result = u.AvatarBody
	case "left_arm":
		result = u.AvatarLArm
	case "right_arm":
		result = u.AvatarRArm
	case "left_leg":
		result = u.AvatarLLeg
	case "right_leg":
		result = u.AvatarRLeg
	case "left_hand":
		result = u.AvatarLHand
	case "right_hand":
		result = u.AvatarRHand
	case "left_foot":
		result = u.AvatarLFoot
	case "right_foot":
		result = u.AvatarRFoot
	default:
		err = errors.New("body part name not recognized")
	}

	return result, err
}

// GetChosenEquipmentName ...
func (u *User) GetChosenEquipmentName(name string) (result string, err error) {
	if u == nil {
		err = errors.New("user is nil")
		fmt.Println("[GetChosenEquipmentName] ERROR:", err.Error())
		return
	}

	err = nil
	result = ""

	// // make sure `name` is the name of an equipment, or return an error
	// if item.IsItemNameForEquipment(name) == false {
	// 	return "", errors.New("provided name is not an equipment name")
	// }

	switch name {
	case "top":
		result = u.AvatarTop
	case "lsleeve":
		result = u.AvatarLSleeve
	case "rsleeve":
		result = u.AvatarRSleeve
	case "lpant":
		result = u.AvatarLPant
	case "rpant":
		result = u.AvatarRPant
	case "lglove":
		result = u.AvatarLGlove
	case "rglove":
		result = u.AvatarRGlove
	case "lboot":
		result = u.AvatarLBoot
	case "rboot":
		result = u.AvatarRBoot
		// v3
	case "boots":
		result = u.AvatarBoots
	case "hair":
		result = u.AvatarHair
	case "jacket":
		result = u.AvatarJacket
	case "pants":
		result = u.AvatarPants
	case "shirt":
		result = u.AvatarShirt
	default:
		err = errors.New("equipment name not recognized")
	}

	return result, err
}

// Sorting

type usersByCreationDate []*User

func (arr usersByCreationDate) Len() int {
	return len(arr)
}

func (arr usersByCreationDate) Less(i, j int) bool {
	return (*(arr[i].Created)).Before(*(arr[j].Created))
}

func (arr usersByCreationDate) Swap(i, j int) {
	arr[i], arr[j] = arr[j], arr[i]
}

func SortByCreationDate(arr []*User) {
	sort.Sort(usersByCreationDate(arr))
}

// Utility functions

func SanitizeEmail(email string) string {
	return strings.TrimSpace(strings.ToLower(email))
}

func RemoveTokens(tokens []Token, invalidTokens []Token) []Token {
	if len(invalidTokens) == 0 {
		return tokens
	}
	// remove invalid tokens
	invalidTokensMap := make(map[string]bool)
	for _, t := range invalidTokens {
		invalidTokensMap[t.Token] = true
	}
	validTokens := make([]Token, 0)
	for _, t := range tokens {
		if !invalidTokensMap[t.Token] {
			validTokens = append(validTokens, t)
		}
	}
	return validTokens
}

func containsString(slice []string, str string) bool {
	for _, item := range slice {
		if item == str {
			return true
		}
	}
	return false
}
