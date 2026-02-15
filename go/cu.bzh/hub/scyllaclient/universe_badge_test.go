package scyllaclient

import (
	"fmt"
	"testing"
	"time"

	"github.com/google/uuid"
)

func getClient() *Client {
	client, err := New(ClientConfig{
		Servers:  []string{"localhost:9042"},
		Keyspace: "universe",
	})
	if err != nil {
		fmt.Println("❌", err.Error())
		return nil
	}
	return client
}

func TestBadgeOperations(t *testing.T) {

	scyllaClient := getClient()

	// count badges
	var err error
	var count int
	{
		count, err = scyllaClient.CountAllBadges()
		if err != nil {
			t.Error(err)
		}
	}

	badgeID := uuid.New().String()
	worldID := uuid.New().String()
	tag := "test"
	name := "testName"
	description := "testDescription"
	// hiddenIfLocked := false
	// rarity := float32(0.5)
	createdAt := time.Now()
	updatedAt := createdAt

	// write new record
	{
		_, inserted, err := scyllaClient.InsertBadge(Badge{
			BadgeID:     badgeID,
			WorldID:     worldID,
			Tag:         tag,
			Name:        name,
			Description: description,
			// HiddenIfLocked: hiddenIfLocked,
			// Rarity:         rarity,
			CreatedAt: createdAt,
			UpdatedAt: updatedAt,
		})
		if err != nil {
			t.Error(err)
		}
		if !inserted {
			t.Error("badge should be inserted")
		}

		newCount, err := scyllaClient.CountAllBadges()
		if err != nil {
			t.Error(err)
		}
		if newCount != count+1 {
			t.Error("count should be incremented")
		}
		count = newCount
	}

	// write same worldID and tag (this should fail)
	{
		_, inserted, err := scyllaClient.InsertBadge(Badge{
			BadgeID:     uuid.New().String(),
			WorldID:     worldID,
			Tag:         tag,
			Name:        name + "_alt",
			Description: description + "_alt",
			// HiddenIfLocked: hiddenIfLocked,
			// Rarity:         rarity,
			CreatedAt: createdAt,
			UpdatedAt: updatedAt,
		})
		if err != nil {
			t.Error(err)
		}
		if inserted {
			t.Error("badge should not be inserted")
		}

		newCount, err := scyllaClient.CountAllBadges()
		if err != nil {
			t.Error(err)
		}
		if newCount != count {
			t.Error("count should not have increased")
		}
	}

	// read record
	{
		read, err := scyllaClient.GetBadgeByID(badgeID)
		if err != nil {
			t.Error(err)
		}
		if read.BadgeID != badgeID {
			t.Error("badgeID should be the same")
		}
		if read.WorldID != worldID {
			t.Error("worldID should be the same")
		}
		if read.Tag != tag {
			t.Error("tag should be the same")
		}
		if read.Name != name {
			t.Error("name should be the same")
		}
		if read.Description != description {
			t.Error("description should be the same")
		}
		if !read.CreatedAt.Equal(createdAt.UTC().Truncate(time.Millisecond)) {
			t.Error("createdAt should be the same")
		}
		if !read.UpdatedAt.Equal(updatedAt.UTC().Truncate(time.Millisecond)) {
			t.Error("updatedAt should be the same")
		}
	}

	// update record
	{
		newName := name + "_modified"
		newDescription := description + "_modified"
		err = scyllaClient.UpdateBadgeByWorldIDAndTag(worldID, tag, BadgeUpdate{
			Name:        &newName,
			Description: &newDescription,
			// HiddenIfLocked: &hiddenIfLocked,
			// Rarity:         &rarity,
			UpdatedAt: time.Now(),
		})
		if err != nil {
			t.Error(err)
		}

		read, err := scyllaClient.GetBadgeByID(badgeID)
		if err != nil {
			t.Error(err)
		}
		if read.Name != newName {
			t.Error("name should be the same")
		}
		if read.Description != newDescription {
			t.Error("description should be the same")
		}

		read, err = scyllaClient.GetBadgeByWorldIDAndTag(worldID, tag)
		if err != nil {
			t.Error(err)
		}
		if read.Name != newName {
			t.Error("name should be the same")
		}
		if read.Description != newDescription {
			t.Error("description should be the same")
		}
	}

	// get badges by worldID
	{
		badges, err := scyllaClient.GetBadgesByWorldID(worldID)
		if err != nil {
			t.Error(err)
		}
		if len(badges) != 1 {
			t.Error("badges should be 1")
		}
		if badges[0].WorldID != worldID {
			t.Error("worldID should be the same")
		}
		if badges[0].BadgeID != badgeID {
			t.Error("badgeID should be the same")
		}
		if badges[0].Tag != tag {
			t.Error("tag should be the same")
		}
	}
}

func TestUserBadgeOperations(t *testing.T) {

	scyllaClient := getClient()

	// count user badges
	var err error
	var initialCount int
	{
		initialCount, err = scyllaClient.CountAllUserBadges()
		if err != nil {
			t.Error(err)
		}
	}

	userID1 := uuid.New().String()
	badgeID1 := uuid.New().String()

	userID2 := uuid.New().String()
	badgeID2 := uuid.New().String()

	// insert new user badge
	{
		err := scyllaClient.InsertUserBadge(userID1, badgeID1)
		if err != nil {
			t.Error(err)
		}
		err = scyllaClient.InsertUserBadge(userID2, badgeID2)
		if err != nil {
			t.Error(err)
		}

		newCount, err := scyllaClient.CountAllUserBadges()
		if err != nil {
			t.Error(err)
		}
		if newCount != initialCount+2 {
			t.Error("count should be incremented by 2")
		}
	}

	// count user badges by badgeID
	{
		count, err := scyllaClient.CountUserBadgesByID(badgeID1)
		if err != nil {
			t.Error(err)
		}
		if count != 1 {
			t.Error("count should be 1")
		}
		count, err = scyllaClient.CountUserBadgesByID(badgeID2)
		if err != nil {
			t.Error(err)
		}
		if count != 1 {
			t.Error("count should be 1")
		}
	}

	// get user badge by userID and badgeID
	{
		userBadge, err := scyllaClient.GetUserBadge(userID1, badgeID1)
		if err != nil {
			t.Error(err)
		}
		if userBadge.UserID != userID1 {
			t.Error("userID should be the same")
		}
		if userBadge.BadgeID != badgeID1 {
			t.Error("badgeID should be the same")
		}

		userBadge, err = scyllaClient.GetUserBadge(userID2, badgeID2)
		if err != nil {
			t.Error(err)
		}
		if userBadge.UserID != userID2 {
			t.Error("userID should be the same")
		}
		if userBadge.BadgeID != badgeID2 {
			t.Error("badgeID should be the same")
		}
	}

	// get badges by userID
	{
		badges, err := scyllaClient.GetUserBadgesByUserID(userID1)
		if err != nil {
			t.Error(err)
		}
		if len(badges) != 1 {
			t.Error("badges should be 1")
		}
		if badges[0].UserID != userID1 {
			t.Error("userID should be the same")
		}
		if badges[0].BadgeID != badgeID1 {
			t.Error("badgeID should be the same")
		}

		badges, err = scyllaClient.GetUserBadgesByUserID(userID2)
		if err != nil {
			t.Error(err)
		}
		if len(badges) != 1 {
			t.Error("badges should be 1")
		}
		if badges[0].UserID != userID2 {
			t.Error("userID should be the same")
		}
		if badges[0].BadgeID != badgeID2 {
			t.Error("badgeID should be the same")
		}
	}
}
