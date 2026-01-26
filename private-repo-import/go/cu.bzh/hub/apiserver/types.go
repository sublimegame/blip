package main

import (
	"fmt"
	"reflect"
	"slices"
	"strings"
	"time"

	"cu.bzh/hub/game"
	"cu.bzh/hub/item"
	"cu.bzh/hub/scyllaclient"
	"cu.bzh/hub/scyllaclient/model/kvs"
	"cu.bzh/hub/search"
	"cu.bzh/hub/user"
	"cu.bzh/types/gameserver"
)

// ------------------------------
// User
// ------------------------------

type userPatch struct {
	// bio and other profile fields
	Bio                  *string `json:"bio,omitempty"`
	Discord              *string `json:"discord,omitempty"`
	X                    *string `json:"x,omitempty"`
	Tiktok               *string `json:"tiktok,omitempty"`
	Website              *string `json:"website,omitempty"`
	Github               *string `json:"github,omitempty"`
	NewPassword          *string `json:"newPassword,omitempty"`
	CurrentPassword      *string `json:"currentPassword,omitempty"`
	Email                *string `json:"email,omitempty"`
	EmailVerifCode       *string `json:"emailVerifCode,omitempty"`
	ParentEmail          *string `json:"parentEmail,omitempty"`
	ParentEmailVerifCode *string `json:"parentEmailVerifCode,omitempty"`
	// TEMPORARY (for pre-0.0.52 users to complete their accounts)
	Username             *string `json:"username,omitempty"`
	UsernameKey          *string `json:"usernameKey,omitempty"`
	Phone                *string `json:"phone,omitempty"`
	PhoneVerifCode       *string `json:"phoneVerifCode,omitempty"`
	ParentPhone          *string `json:"parentPhone,omitempty"`
	ParentPhoneVerifCode *string `json:"parentPhoneVerifCode,omitempty"`
	DateOfBirth          *string `json:"dob,omitempty"` // mm-dd-yyyy format
	Age                  *int32  `json:"age,omitempty"`
	PasskeyPublicKey     *string `json:"passkeyPublicKey,omitempty"`
}

// userInfoMinimal
type userInfoMinimal struct {
	ID       string `json:"id"`
	Username string `json:"username"`
}

// Types used for user/avatar requests/responses

type avatar struct {
	SkinColorIndex *int    `json:"skinColorIndex"`
	EyesColorIndex *int    `json:"eyesColorIndex"`
	EyesIndex      *int    `json:"eyesIndex"`
	NoseIndex      *int    `json:"noseIndex"`
	Hair           *string `json:"hair"`
	Jacket         *string `json:"jacket"`
	Pants          *string `json:"pants"`
	Boots          *string `json:"boots"`
	// LEGACY (for now)
	EyesColor  *user.Color `json:"eyesColor"`
	NoseColor  *user.Color `json:"noseColor"`
	SkinColor  *user.Color `json:"skinColor"`
	SkinColor2 *user.Color `json:"skinColor2"`
	MouthColor *user.Color `json:"mouthColor"`
}

func (a *avatar) ToString() string {
	if a == nil {
		return "[NULL avatar]"
	}
	return fmt.Sprintf(
		`Avatar{
		SkinColorIndex: %v,
		EyesColorIndex: %v,
		EyesIndex: %v,
		NoseIndex: %v,
		Hair: %v,
		Jacket: %v,
	 	Pants: %v,
		Boots: %v,
		EyesColor: %v,
		NoseColor: %v,
		SkinColor: %v,
		SkinColor2: %v,
		MouthColor: %v}`,
		a.SkinColorIndex, a.EyesColorIndex, a.EyesIndex, a.NoseIndex, a.Hair, a.Jacket, a.Pants, a.Boots, a.EyesColor, a.NoseColor, a.SkinColor, a.SkinColor2, a.MouthColor)
}

// avatar struct constructor
func NewAvatar(skinColorIdx, eyesColorIdx, eyesIdx, nodeIdx *int, boots, hair, jacket, pants *string, eyesColor, noseColor, skinColor, skinColor2, mouthColor *user.Color) avatar {
	return avatar{
		SkinColorIndex: skinColorIdx,
		EyesColorIndex: eyesColorIdx,
		EyesIndex:      eyesIdx,
		NoseIndex:      nodeIdx,
		Hair:           hair,
		Jacket:         jacket,
		Pants:          pants,
		Boots:          boots,
		// LEGACY
		EyesColor:  eyesColor,
		NoseColor:  noseColor,
		SkinColor:  skinColor,
		SkinColor2: skinColor2,
		MouthColor: mouthColor,
	}
}

//

type UserCredentials struct {
	// TODO: gaetan: this can be removed when not needed anymore (approx build #208)
	UserIDOld string `json:"user-id,omitempty"`
	// The JSON field name is `userID` to be easier to use in Luau (in the client)
	UserIDNew string `json:"userID,omitempty"`
	// UserID + token is enough to be connected
	// token is created in DB when login in
	// or shared when login in from multiple devices.
	// It can be revoked, in that case, all devices end
	// up being disconnected
	Token string `json:"token,omitempty"`
}

func NewUserCredentials(userID, token string) *UserCredentials {
	return &UserCredentials{
		UserIDOld: userID,
		UserIDNew: userID,
		Token:     token,
	}
}

type WorldServer struct {
	ID         string `json:"id,omitempty"`
	Address    string `json:"address,omitempty"`
	Players    int32  `json:"players,omitempty"`
	MaxPlayers int32  `json:"max-players,omitempty"`
	DevMode    bool   `json:"dev-mode"`
}

// HTTP interface, not used to parse DB results
type World struct {
	ID               string   `json:"id,omitempty"` // ID should be empty when creating a new game
	AuthorID         string   `json:"author-id,omitempty"`
	AuthorName       string   `json:"author-name,omitempty"`
	Contributors     []string `json:"contributors,omitempty"`
	ContributorNames []string `json:"contributor-names,omitempty"`
	Featured         bool     `json:"featured,omitempty"`
	Likes            int32    `json:"likes,omitempty"`
	Views            int32    `json:"views,omitempty"`
	Title            string   `json:"title,omitempty"`
	Description      string   `json:"description,omitempty"`
	Tags             []string `json:"tags,omitempty"`
	Liked            bool     `json:"liked,omitempty"`          // true if game has been liked by the player who requested the game
	IsAuthor         bool     `json:"is-author,omitempty"`      // true if requester is the author of the game
	IsContributor    bool     `json:"is-contributor,omitempty"` // true if requester is a contributor
	Script           string   `json:"script,omitempty"`
	MapBase64        string   `json:"mapBase64,omitempty"`
	MaxPlayers       int32    `json:"maxPlayers,omitempty"`
}

type WorldV2 struct {
	ID               string     `json:"id,omitempty"` // ID should be empty when creating a new game
	AuthorID         string     `json:"authorId,omitempty"`
	AuthorName       string     `json:"authorName,omitempty"`
	Contributors     []string   `json:"contributors,omitempty"`
	ContributorNames []string   `json:"contributorNames,omitempty"`
	Created          *time.Time `json:"created,omitempty"`
	Updated          *time.Time `json:"updated,omitempty"`
	Featured         bool       `json:"featured,omitempty"`
	Likes            int32      `json:"likes,omitempty"`
	Views            int32      `json:"views,omitempty"`
	Title            string     `json:"title,omitempty"`
	Description      string     `json:"description,omitempty"`
	Tags             []string   `json:"tags,omitempty"`
	Liked            bool       `json:"liked,omitempty"`         // true if game has been liked by the player who requested the game
	IsAuthor         bool       `json:"isAuthor,omitempty"`      // true if requester is the author of the game
	IsContributor    bool       `json:"isContributor,omitempty"` // true if requester is a contributor
	Script           string     `json:"script,omitempty"`
	MapBase64        string     `json:"mapBase64,omitempty"`
	MaxPlayers       int32      `json:"maxPlayers,omitempty"`
}

func newWorldV2FromSearchResult(sr *search.World, usr *user.User, fieldsToKeep []string) *WorldV2 {
	createdPtr := time.UnixMicro(sr.CreatedAt)
	updatedPtr := time.UnixMicro(sr.UpdatedAt)

	w := &WorldV2{
		ID:         sr.ID,
		AuthorID:   sr.AuthorID,
		AuthorName: sr.AuthorName,
		// Contributors
		// ContributorNames
		Featured: sr.Featured,
		Likes:    sr.Likes,
		Views:    sr.Views,
		Title:    sr.Title,
		// Description
		// Tags
		// Liked
		IsAuthor: usr != nil && sr.AuthorID == usr.ID,
		// IsContributor
		// Script
		// MapBase64
		// MaxPlayers
		Created: &createdPtr,
		Updated: &updatedPtr,
	}

	// Filter on fields.
	// If `fields` is nil, nothing is changed (all fields are kept).
	w.keepOnlyFieldsV2(fieldsToKeep)

	return w
}

// Constructor for WorldV2 using a game.Game object
func newWorldV2FromGame(g *game.Game, usr *user.User, fieldsToKeep []string) *WorldV2 {
	w := &WorldV2{
		ID:           g.ID,
		AuthorID:     g.Author,
		AuthorName:   g.AuthorName,
		Contributors: g.Contributors,
		Created:      &g.Created,
		Updated:      &g.Updated,
		// ContributorNames
		Featured:    g.Featured,
		Likes:       g.Likes,
		Views:       g.Views,
		Title:       g.Title,
		Description: g.Description,
		Tags:        g.Tags,
		Liked:       g.Liked,
		IsAuthor:    usr != nil && g.Author == usr.ID,
		// IsContributor
		Script:     g.Script,
		MapBase64:  g.MapBase64,
		MaxPlayers: g.MaxPlayers,
	}

	// Filter on fields.
	// If `fields` is nil, nothing is changed (all fields are kept).
	w.keepOnlyFieldsV2(fieldsToKeep)

	return w
}

func (w *WorldV2) keepOnlyFieldsV2(fieldsToKeep []string) error {
	// If fieldsToKeep is nil, keep all fields (no change)
	if fieldsToKeep == nil {
		fmt.Println("⚠️ [keepOnlyFieldsV2] fieldsToKeep argument is nil")
		return nil // success, nothing was changed
	}

	// process all fields of Worlds and set them to nil if they are not part of fields array
	val := reflect.ValueOf(w)

	if val.Kind() == reflect.Ptr {
		val = val.Elem()
	}

	if val.Kind() != reflect.Struct {
		return fmt.Errorf("obj must be a struct or a pointer to a struct")
	}

	typ := val.Type()

	for i := 0; i < val.NumField(); i++ {
		var field reflect.StructField = typ.Field(i)
		jsonTagInStruct, tagFoundInStruct := field.Tag.Lookup("json")
		if tagFoundInStruct {
			jsonTagInStruct = strings.TrimSuffix(jsonTagInStruct, ",omitempty")
			if !slices.Contains(fieldsToKeep, jsonTagInStruct) {
				ff := val.FieldByName(field.Name)
				// set field to its "zero" value
				if ff.CanSet() {
					ff.Set(reflect.Zero(field.Type))
				}
			}
		}
	}

	return nil
}

// TODO: gdevillele: Use standard item type and extend it instead of duplicating code.
//                   Something like this:
// type APIItem struct {
// 	item.Item
// 	AuthorName string `json:"author-name,omitempty"`
// }

// Used for build versions <=171.
// Remove when not used anymore.
type Item struct {
	ID               string   `json:"id,omitempty"`
	AuthorID         string   `json:"author-id,omitempty"`
	AuthorName       string   `json:"author-name,omitempty"`
	Contributors     []string `json:"contributors,omitempty"`
	ContributorNames []string `json:"contributor-names,omitempty"`
	Repo             string   `json:"repo,omitempty"`
	Name             string   `json:"name,omitempty"`
	Public           bool     `json:"public,omitempty"`
	Description      string   `json:"description,omitempty"`
	Featured         bool     `json:"featured,omitempty"`
	Likes            int32    `json:"likes,omitempty"`
	Views            int32    `json:"views,omitempty"`
	Tags             []string `json:"tags,omitempty"`
	Category         string   `json:"category,omitempty"`
	Liked            bool     `json:"liked,omitempty"` // true if item has been liked by the player who requested it
}

// Used for build versions >171
type ItemV2 struct {
	ID               string     `json:"id,omitempty"`
	AuthorID         string     `json:"authorId,omitempty"`
	AuthorName       string     `json:"authorName,omitempty"`
	Contributors     []string   `json:"contributors,omitempty"`
	ContributorNames []string   `json:"contributorNames,omitempty"`
	Repo             string     `json:"repo,omitempty"`
	Name             string     `json:"name,omitempty"`
	Public           bool       `json:"public,omitempty"`
	Description      string     `json:"description,omitempty"`
	Featured         bool       `json:"featured,omitempty"`
	Likes            int32      `json:"likes,omitempty"`
	Views            int32      `json:"views,omitempty"`
	Tags             []string   `json:"tags,omitempty"`
	Category         string     `json:"category,omitempty"`
	Liked            bool       `json:"liked,omitempty"` // true if item has been liked by the player who requested it
	Created          *time.Time `json:"created,omitempty"`
	Updated          *time.Time `json:"updated,omitempty"`
}

// Constructor for WorldV2 using an item.Item object
func newItemV2FromDBItem(itm *item.Item, fields []string) *ItemV2 {
	itmv2 := &ItemV2{
		ID:               itm.ID,
		AuthorID:         itm.Author,
		AuthorName:       "",
		Contributors:     itm.Contributors,
		ContributorNames: []string{},
		Repo:             itm.Repo,
		Name:             itm.Name,
		Public:           itm.Public,
		Description:      itm.Description,
		Featured:         itm.Featured,
		Likes:            itm.Likes,
		Views:            itm.Views,
		Tags:             itm.Tags,
		Category:         itm.Category,
		Liked:            itm.Liked,
		Created:          &itm.Created,
		Updated:          &itm.Updated,
	}

	// Filter on fields.
	// If `fields` is nil, nothing is changed (all fields are kept).
	itmv2.keepOnlyFieldsV2(fields)

	return itmv2
}

func (itm *ItemV2) keepOnlyFieldsV2(fieldsToKeep []string) error {
	// If fieldsToKeep is nil, keep all fields (no change)
	if fieldsToKeep == nil {
		return nil // success, nothing was changed
	}

	// process all fields of Worlds and set them to nil if they are not part of fields array
	val := reflect.ValueOf(itm)

	if val.Kind() == reflect.Ptr {
		val = val.Elem()
	}

	if val.Kind() != reflect.Struct {
		return fmt.Errorf("obj must be a struct or a pointer to a struct")
	}

	typ := val.Type()

	for i := 0; i < val.NumField(); i++ {
		var field reflect.StructField = typ.Field(i)
		jsonTagInStruct, tagFoundInStruct := field.Tag.Lookup("json")
		if tagFoundInStruct {
			jsonTagInStruct = strings.TrimSuffix(jsonTagInStruct, ",omitempty")
			if !slices.Contains(fieldsToKeep, jsonTagInStruct) {
				ff := val.FieldByName(field.Name)
				// set field to its "zero" value
				if ff.CanSet() {
					ff.Set(reflect.Zero(field.Type))
				}
			}
		}
	}

	return nil
}

type getLoginOptionReq struct {
	UsernameOrEmail    string `json:"usernameOrEmail,omitempty"`
	UsernameOrEmailOld string `json:"username-or-email,omitempty"` // remove when build 207 is not used anymore
}

// remove when build 207 is not used anymore
func (r *getLoginOptionReq) GetUsernameOrEmail() string {
	res := r.UsernameOrEmail
	if res == "" {
		res = r.UsernameOrEmailOld
	}
	return res
}

type getLoginOptionRes struct {
	Username                   string   `json:"username,omitempty"`
	Password                   bool     `json:"password,omitempty"`
	Passkey                    bool     `json:"passkey,omitempty"`
	MagicKey                   bool     `json:"magickey,omitempty"`
	MagicKeyDeliveryMethods    []string `json:"magickeyDeliveryMethods,omitempty"`
	MagicKeyOld                bool     `json:"magic-key,omitempty"`               // remove when build 207 is not used anymore
	MagicKeyDeliveryMethodsOld []string `json:"magicKeyDeliveryMethods,omitempty"` // remove when build 207 is not used anymore
}

func NewGetLoginOptionRes(usr *user.User) (*getLoginOptionRes, error) {
	if usr == nil {
		return nil, fmt.Errorf("user is nil")
	}

	passwordAvailable := usr.Password != ""
	passkeyAvailable := usr.PasskeyAAGUID != "" && usr.PasskeyCredentialID != "" && usr.PasskeyPublicKey != ""
	magicKeyAvailable := false
	magicKeyDeliveryMethods := []string{}

	if usr.Email != "" {
		magicKeyAvailable = true
		magicKeyDeliveryMethods = append(magicKeyDeliveryMethods, "email")
	}
	if usr.ParentEmail != "" {
		magicKeyAvailable = true
		magicKeyDeliveryMethods = append(magicKeyDeliveryMethods, "parentEmail")
	}
	if usr.Phone != nil && *usr.Phone != "" {
		magicKeyAvailable = true
		magicKeyDeliveryMethods = append(magicKeyDeliveryMethods, "phone")
	}
	if usr.ParentPhone != nil && *usr.ParentPhone != "" {
		magicKeyAvailable = true
		magicKeyDeliveryMethods = append(magicKeyDeliveryMethods, "parentPhone")
	}

	return &getLoginOptionRes{
		Username:                   usr.Username,
		Password:                   passwordAvailable,
		Passkey:                    passkeyAvailable,
		MagicKey:                   magicKeyAvailable,
		MagicKeyDeliveryMethods:    magicKeyDeliveryMethods,
		MagicKeyOld:                magicKeyAvailable,
		MagicKeyDeliveryMethodsOld: magicKeyDeliveryMethods,
	}, nil
}

type getMagicKeyReq struct {
	UsernameOrEmail string `json:"username-or-email,omitempty"`
}

type loginReq struct {
	UsernameOrEmail                string `json:"username-or-email,omitempty"`
	Password                       string `json:"password,omitempty"`
	MagicKey                       string `json:"magic-key,omitempty"`
	PasskeyCredentialIDBase64      string `json:"passkeyCredentialIDBase64,omitempty"`
	PasskeyAuthenticatorDataBase64 string `json:"passkeyAuthenticatorDataBase64,omitempty"`
	PasskeyRawClientDataJSONString string `json:"passkeyRawClientDataJSONString,omitempty"`
	PasskeySignatureBase64         string `json:"passkeySignatureBase64,omitempty"`
	PasskeyUserIDString            string `json:"passkeyUserIDString,omitempty"`
}

type loginRes struct {
	Username        string           `json:"username,omitempty"`
	Credentials     *UserCredentials `json:"credentials,omitempty"`
	HasUsername     bool             `json:"has-username,omitempty"`
	HasDOB          bool             `json:"has-dob,omitempty"`
	HasEstimatedDOB bool             `json:"has-estimated-dob,omitempty"`
	HasPassword     bool             `json:"has-password,omitempty"`
	HasEmail        bool             `json:"has-email,omitempty"`
	Under13         bool             `json:"under-13,omitempty"`
	AvatarHead      string           `json:"avatarHead,omitempty"`
	AvatarBody      string           `json:"avatarBody,omitempty"`
	AvatarLArm      string           `json:"avatarLArm,omitempty"`
	AvatarRArm      string           `json:"avatarRArm,omitempty"`
	AvatarLHand     string           `json:"avatarLHand,omitempty"`
	AvatarRHand     string           `json:"avatarRHand,omitempty"`
	AvatarLLeg      string           `json:"avatarLLeg,omitempty"`
	AvatarRLeg      string           `json:"avatarRLeg,omitempty"`
	AvatarLFoot     string           `json:"avatarLFoot,omitempty"`
	AvatarRFoot     string           `json:"avatarRFoot,omitempty"`
	AvatarHair      string           `json:"avatarHair,omitempty"`
	AvatarTop       string           `json:"avatarTop,omitempty"`
	AvatarLSleeve   string           `json:"avatarLSleeve,omitempty"`
	AvatarRSleeve   string           `json:"avatarRSleeve,omitempty"`
	AvatarLGlove    string           `json:"avatarLGlove,omitempty"`
	AvatarRGlove    string           `json:"avatarRGlove,omitempty"`
	AvatarLPant     string           `json:"avatarLPant,omitempty"`
	AvatarRPant     string           `json:"avatarRPant,omitempty"`
	AvatarLBoot     string           `json:"avatarLBoot,omitempty"`
	AvatarRBoot     string           `json:"avatarRBoot,omitempty"`
}

type isLoggedInReq struct {
	Credentials *UserCredentials `json:"credentials,omitempty"`
}

type isLoggedInRes struct {
	IsLoggedIn      bool   `json:"is-logged-in,omitempty"`
	Username        string `json:"username,omitempty"`
	HasUsername     bool   `json:"has-username,omitempty"`
	HasDOB          bool   `json:"has-dob,omitempty"`
	HasEstimatedDOB bool   `json:"has-estimated-dob,omitempty"`
	HasPassword     bool   `json:"has-password,omitempty"`
	HasEmail        bool   `json:"has-email,omitempty"`
	Under13         bool   `json:"under-13,omitempty"` // makes sense only if hasDOB == true or HasEstimatedDOB == true
}

type connectReq struct {
	Credentials *UserCredentials `json:"credentials,omitempty"`
}

type connectRes struct {
	Connected       bool             `json:"connected,omitempty"`
	Username        string           `json:"username,omitempty"`
	HasDOB          bool             `json:"has-dob,omitempty"`
	HasEstimatedDOB bool             `json:"has-estimated-dob,omitempty"`
	HasPassword     bool             `json:"has-password,omitempty"`
	HasEmail        bool             `json:"has-email,omitempty"`
	Under13         bool             `json:"under-13,omitempty"` // makes sense only if hasDOB == true or HasEstimatedDOB == true
	Credentials     *UserCredentials `json:"credentials,omitempty"`
	AvatarHead      string           `json:"avatarHead,omitempty"`
	AvatarBody      string           `json:"avatarBody,omitempty"`
	AvatarLArm      string           `json:"avatarLArm,omitempty"`
	AvatarRArm      string           `json:"avatarRArm,omitempty"`
	AvatarLHand     string           `json:"avatarLHand,omitempty"`
	AvatarRHand     string           `json:"avatarRHand,omitempty"`
	AvatarLLeg      string           `json:"avatarLLeg,omitempty"`
	AvatarRLeg      string           `json:"avatarRLeg,omitempty"`
	AvatarLFoot     string           `json:"avatarLFoot,omitempty"`
	AvatarRFoot     string           `json:"avatarRFoot,omitempty"`
	AvatarHair      string           `json:"avatarHair,omitempty"`
	AvatarTop       string           `json:"avatarTop,omitempty"`
	AvatarLSleeve   string           `json:"avatarLSleeve,omitempty"`
	AvatarRSleeve   string           `json:"avatarRSleeve,omitempty"`
	AvatarLGlove    string           `json:"avatarLGlove,omitempty"`
	AvatarRGlove    string           `json:"avatarRGlove,omitempty"`
	AvatarLPant     string           `json:"avatarLPant,omitempty"`
	AvatarRPant     string           `json:"avatarRPant,omitempty"`
	AvatarLBoot     string           `json:"avatarLBoot,omitempty"`
	AvatarRBoot     string           `json:"avatarRBoot,omitempty"`
	// Avatar      Avatar        `json:"avatar,omitempty"`
	// Equipment Equipment       `json:"equipment,omitempty"`
}

type worldEditReq struct {
	Credentials *UserCredentials `json:"credentials,omitempty"`
	World       *World           `json:"world,omitempty"` // World with info to set or update (title, description) (other fields left empty)
}

type getWorldRes struct {
	World *World `json:"world,omitempty"`
}

// used for both worldServers & worldRunningServer
type worldServerReq struct {
	Credentials *UserCredentials `json:"credentials,omitempty"`
	WorldID     string           `json:"world-id,omitempty"`
	ServerTag   string           `json:"server-tag,omitempty"` // optional, to request a specific image tag
	Mode        gameserver.Mode  `json:"mode,omitempty"`
}

type getItemRes struct {
	Item *Item `json:"item,omitempty"`
}

type itemEditReq struct {
	Credentials *UserCredentials `json:"credentials,omitempty"` // later, we will be able to remove this (creds are now in HTTP headers)
	Item        *Item            `json:"world,omitempty"`       // Item with info to set or update (description) (other fields left empty)
	Original    string           `json:"original,omitempty"`    // provided when duplicating an item
}

type FriendRequest struct {
	SenderID    string `json:"senderID,omitempty"`
	RecipientID string `json:"recipientID,omitempty"`
}

// {
// id="09c5cd9e-9c3a-4dc5-8083-06b77e1095e3",
// from={id="209809842",name="caillef"},
// to={id="20980242",name="gdevillele"},
// amount=283,
// action="buy",
// item={ id="09b5cd9f-9c3a-4dc5-8083-06b77e1099e5", slug="caillef.shop" },
// copy=273,
// date="2020-07-12 17:01:00.000" }
type UserTransaction struct {
	ID       string    `json:"id,omitempty"`
	From     user.User `json:"from,omitempty"`
	To       user.User `json:"to,omitempty"`
	Amount   uint64    `json:"amount,omitempty"`
	Action   string    `json:"action,omitempty"`
	Item     Item      `json:"item,omitempty"`
	Copy     uint64    `json:"copy,omitempty"`
	DateTime time.Time `json:"date,omitempty"`
}

// ------------------------------
// USER INFO
// ------------------------------
// userInfo is a struct representing a user.User, used for HTTP API client/server communication
type userInfo struct {
	ID                     *string    `json:"id,omitempty"`
	Username               *string    `json:"username,omitempty"`
	Created                *time.Time `json:"created,omitempty"`
	Updated                *time.Time `json:"updated,omitempty"`
	LastSeen               *time.Time `json:"lastSeen,omitempty"`
	NbFriends              *int32     `json:"nbFriends,omitempty"`
	HasUsername            *bool      `json:"hasUsername,omitempty"`
	HasEmail               *bool      `json:"hasEmail,omitempty"`
	HasPassword            *bool      `json:"hasPassword,omitempty"`
	HasPasskey             *bool      `json:"hasPasskey,omitempty"`
	HasDOB                 *bool      `json:"hasDOB,omitempty"`
	HasEstimatedDOB        *bool      `json:"hasEstimatedDOB,omitempty"`
	HasAcceptedTerms       *bool      `json:"hasAcceptedTerms,omitempty"`
	IsUnder13              *bool      `json:"isUnder13,omitempty"` // under 13 disclaimer is saved on device (to be approved on each new install)
	HasVerifiedPhoneNumber *bool      `json:"hasVerifiedPhoneNumber,omitempty"`
	DidCustomizeAvatar     *bool      `json:"didCustomizeAvatar,omitempty"`
	Bio                    *string    `json:"bio,omitempty"`
	Discord                *string    `json:"discord,omitempty"`
	X                      *string    `json:"x,omitempty"`
	Tiktok                 *string    `json:"tiktok,omitempty"`
	Website                *string    `json:"website,omitempty"`
	Github                 *string    `json:"github,omitempty"`
	Email                  *string    `json:"email,omitempty" czhPerm:"elevated"`
	EmailTemporary         *string    `json:"emailTemporary,omitempty" czhPerm:"elevated"`
	ParentEmail            *string    `json:"parentEmail,omitempty" czhPerm:"elevated"`
	ParentEmailTemporary   *string    `json:"parentEmailTemporary,omitempty" czhPerm:"elevated"`
	// Computed fields
	IsPhoneExempted          *bool `json:"isPhoneExempted,omitempty"`
	IsParentApproved         *bool `json:"isParentApproved,omitempty"`
	HasUnverifiedPhoneNumber *bool `json:"hasUnverifiedPhoneNumber,omitempty"`
	IsChatEnabled            *bool `json:"isChatEnabled,omitempty"`

	// blocked users
	BlockedUsers []string `json:"blockedUsers,omitempty" czhPerm:"elevated"`
}

// userInfoFields contains the names of the userInfo fields (json tags)
var userInfoFields = []string{}

// userInfoFieldsElevated contains the names of the userInfo fields (json tags) that are only available to users with elevated privileges
var userInfoFieldsElevated = []string{}

// userInfoFieldsInit initializes the userInfoFields slice with the names of the userInfo fields (json tags)
func userInfoFieldsInit() {
	if len(userInfoFields) > 0 {
		return
	}

	// loop over userInfo struct fields and get their json tags
	{
		val := reflect.ValueOf(userInfo{})
		typ := val.Type()
		for i := 0; i < val.NumField(); i++ {
			var field reflect.StructField = typ.Field(i)
			jsonTagInStruct, tagFoundInStruct := field.Tag.Lookup("json")
			if tagFoundInStruct {
				// keep part before comma (if any)
				jsonTagInStruct = strings.Split(jsonTagInStruct, ",")[0]
				userInfoFields = append(userInfoFields, jsonTagInStruct)
			}
		}
	}

	//
	{
		val := reflect.ValueOf(userInfo{})
		typ := val.Type()
		for i := 0; i < val.NumField(); i++ {
			var field reflect.StructField = typ.Field(i)
			jsonTagInStruct, tagFoundInStruct := field.Tag.Lookup("czhPerm")
			if tagFoundInStruct {
				// keep part before comma (if any)
				jsonTagInStruct = strings.Split(jsonTagInStruct, ",")[0]
				if jsonTagInStruct == "elevated" {
					userInfoFieldsElevated = append(userInfoFieldsElevated, jsonTagInStruct)
				}
			}
		}
	}
}

// populate populates the userInfo object with fields from the user object
func (u *userInfo) populate(usr user.User, fields []string, elevatedRights bool, scyllaClientUniverse *scyllaclient.Client) {

	// initialize userInfoFields if not already done
	userInfoFieldsInit()

	// if fields is empty, populate all fields
	if len(fields) == 0 { // if fields is nil, len(fields) is 0
		fields = userInfoFields
		// if elevatedRights {
		// 	fields = append(fields, "email", "emailTemporary", "parentEmail", "parentEmailTemporary")
		// }
	}

	for _, field := range fields {
		if slices.Contains(userInfoFields, field) {
			// field is valid
			if elevatedRights == false && slices.Contains(userInfoFieldsElevated, field) {
				// field is elevated and user does not have elevated rights
				continue
			}
			switch field {
			case "id":
				u.ID = &usr.ID
			case "username":
				u.Username = &usr.Username
			case "created":
				u.Created = usr.Created
			case "blockedUsers":
				u.BlockedUsers = usr.BlockedUsers
			case "updated":
				u.Updated = usr.Updated
			case "lastSeen":
				u.LastSeen = usr.LastSeen
			case "nbFriends":
				var zero int32 = 0
				u.NbFriends = &zero
				if scyllaClientUniverse != nil {
					count, err := scyllaClientUniverse.CountFriendRelations(usr.ID)
					if err == nil {
						c := int32(count)
						u.NbFriends = &c
					}
				}
			case "hasUsername":
				v := usr.Username != ""
				u.HasUsername = &v
			case "hasEmail":
				v := usr.Email != "" || usr.ParentEmail != ""
				u.HasEmail = &v
			case "hasPassword":
				v := usr.Password != ""
				u.HasPassword = &v
			case "hasDOB":
				v := usr.Dob != nil
				u.HasDOB = &v
			case "hasEstimatedDOB":
				v := usr.EstimatedDob != nil
				u.HasEstimatedDOB = &v
			case "hasAcceptedTerms":
				v := false
				u.HasAcceptedTerms = &v
			case "isUnder13":
				age, err := usr.ComputeExactOrEstimatedAge()
				// if there was an error, we consider the user "under 13"
				v := err != nil || age < 13
				u.IsUnder13 = &v
			case "bio":
				u.Bio = &usr.Bio
			case "discord":
				u.Discord = &usr.Discord
			case "x":
				u.X = &usr.X
			case "tiktok":
				u.Tiktok = &usr.Tiktok
			case "website":
				u.Website = &usr.Website
			case "github":
				u.Github = &usr.Github
			case "email":
				u.Email = &usr.Email
			case "emailTemporary":
				u.EmailTemporary = usr.EmailTemporary
			case "parentEmail":
				u.ParentEmail = &usr.ParentEmail
			case "parentEmailTemporary":
				u.ParentEmailTemporary = usr.ParentEmailTemporary
			case "hasVerifiedPhoneNumber":
				v := (usr.Phone != nil && *usr.Phone != "") || (usr.ParentPhone != nil && *usr.ParentPhone != "")
				u.HasVerifiedPhoneNumber = &v
			case "didCustomizeAvatar":
				v := usr.AvatarEyesColor != nil || usr.AvatarNoseColor != nil || usr.AvatarSkinColor != nil || usr.AvatarSkinColor2 != nil || usr.AvatarMouthColor != nil || usr.AvatarBoots != "" || usr.AvatarHair != "" || usr.AvatarJacket != "" || usr.AvatarPants != ""
				u.DidCustomizeAvatar = &v
			case "isPhoneExempted":
				// user is phone exempted if at least 1 condition is true:
				// - account created before 2024-07-20
				// - has an email address
				// - is a test account
				v := usr.Created.Before(time.Date(2024, 7, 20, 0, 0, 0, 0, time.UTC)) || len(usr.Email) > 0 || usr.IsVerifiedTestAccount
				u.IsPhoneExempted = &v
			case "isParentApproved":
				v := usr.ParentalControlApproved != nil && *usr.ParentalControlApproved == true
				u.IsParentApproved = &v
			case "hasUnverifiedPhoneNumber":
				v := (usr.Phone == nil && usr.PhoneNew != nil && *usr.PhoneNew != "") ||
					(usr.ParentPhone == nil && usr.ParentPhoneNew != nil && *usr.ParentPhoneNew != "") ||
					(usr.IsTestAccount)
				u.HasUnverifiedPhoneNumber = &v
			case "isChatEnabled":
				// must be true by default
				// and false if ParentalControlChatAllowed is NOT NIL and false
				v := usr.ParentalControlChatAllowed == nil || *usr.ParentalControlChatAllowed == true
				u.IsChatEnabled = &v
			}
		}
	}
}

// ----------------------------------------------
// Badge
// ----------------------------------------------

type Badge struct {
	BadgeID        string    `json:"badgeID"`
	WorldID        string    `json:"worldID"`
	Tag            string    `json:"tag"`
	Name           string    `json:"name"`
	Description    string    `json:"description"`
	HiddenIfLocked bool      `json:"hiddenIfLocked,omitempty"`
	Rarity         float32   `json:"rarity,omitempty"`
	CreatedAt      time.Time `json:"createdAt"`
	UpdatedAt      time.Time `json:"updatedAt"`
	//
	UserDidUnlock  bool       `json:"userDidUnlock,omitempty"`  // optional
	UserUnlockedAt *time.Time `json:"userUnlockedAt,omitempty"` // optional
}

func NewBadge(badge scyllaclient.Badge, userBadge *scyllaclient.UserBadge) *Badge {
	result := &Badge{
		BadgeID:        badge.BadgeID,
		WorldID:        badge.WorldID,
		Tag:            badge.Tag,
		Name:           badge.Name,
		Description:    badge.Description,
		HiddenIfLocked: badge.HiddenIfLocked,
		Rarity:         badge.Rarity,
		CreatedAt:      badge.CreatedAt,
		UpdatedAt:      badge.UpdatedAt,
		UserDidUnlock:  false,
		UserUnlockedAt: nil,
	}
	if userBadge != nil {
		result.UserDidUnlock = true
		result.UserUnlockedAt = &userBadge.CreatedAt
	}
	return result
}

type DashboardInfo struct {
	CreatedAt                      *time.Time `json:"createdAt"`
	ChildAge                       *int       `json:"childAge"`
	ParentalControlApproved        *bool      `json:"parentalControlApproved"`
	ParentalControlChatAllowed     *bool      `json:"parentalControlChatAllowed"`
	ParentalControlMessagesAllowed *bool      `json:"parentalControlMessagesAllowed"`
}

type LeaderboardRecord struct {
	//WorldID         string    `json:"worldID"`
	//LeaderboardName string    `json:"leaderboardName"`
	UserID   string    `json:"userID"`
	Score    int64     `json:"score"`
	Value    []byte    `json:"value"`
	Updated  time.Time `json:"updated"`
	Username string    `json:"username,omitempty"`
}

func NewLeaderboardRecord(rec kvs.KvsLeaderboardRecord) LeaderboardRecord {
	return LeaderboardRecord{
		//WorldID:         rec.WorldID,
		//LeaderboardName: rec.LeaderboardName,
		UserID:  rec.UserID,
		Score:   rec.Score,
		Value:   rec.Value,
		Updated: scyllaclient.Int64MsToTime(rec.Updated),
	}
}

type PasskeyClientData struct {
	Type      string `json:"type"`
	Challenge string `json:"challenge"`
	Origin    string `json:"origin"`
}
