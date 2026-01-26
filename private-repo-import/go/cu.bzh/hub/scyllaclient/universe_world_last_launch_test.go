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

func TestInsertWorldLastLaunch(t *testing.T) {

	scyllaClient := getClient()

	worldID := uuid.New().String()
	userID := uuid.New().String()
	updatedAt := time.Now()

	// write new record
	{
		err := scyllaClient.UpsertWorldLastLaunch(WorldLastLaunch{
			WorldID:   worldID,
			UserID:    userID,
			UpdatedAt: updatedAt,
		})
		if err != nil {
			t.Error(err)
		}
	}

	// read record
	{
		read, err := scyllaClient.GetWorldLastLaunch(worldID, userID)
		if err != nil {
			t.Error(err)
		}
		if read.WorldID != worldID {
			t.Error("worldID should be the same")
		}
		if read.UserID != userID {
			t.Error("userID should be the same")
		}
		if !read.UpdatedAt.Equal(updatedAt.UTC().Truncate(time.Millisecond)) {
			t.Error("updatedAt should be the same")
		}
	}
}

func TestUpdateWorldLastLaunch(t *testing.T) {

	scyllaClient := getClient()

	worldID := uuid.New().String()
	userID := uuid.New().String()
	updatedAt := time.Now()

	// write new record
	{
		err := scyllaClient.UpsertWorldLastLaunch(WorldLastLaunch{
			WorldID:   worldID,
			UserID:    userID,
			UpdatedAt: updatedAt,
		})
		if err != nil {
			t.Error(err)
		}
	}

	// read record and check values
	{
		read, err := scyllaClient.GetWorldLastLaunch(worldID, userID)
		if err != nil {
			t.Error(err)
		}
		if read.WorldID != worldID {
			t.Error("worldID should be the same")
		}
		if read.UserID != userID {
			t.Error("userID should be the same")
		}
		if !read.UpdatedAt.Equal(updatedAt.UTC().Truncate(time.Millisecond)) {
			t.Error("updatedAt should be the same")
		}
	}

	// update record with new updatedAt value
	time.Sleep(time.Duration(100 * time.Millisecond))
	updatedAt = time.Now()
	{
		err := scyllaClient.UpsertWorldLastLaunch(WorldLastLaunch{
			WorldID:   worldID,
			UserID:    userID,
			UpdatedAt: updatedAt,
		})
		if err != nil {
			t.Error(err)
		}
	}

	// read record and check values
	{
		read, err := scyllaClient.GetWorldLastLaunch(worldID, userID)
		if err != nil {
			t.Error(err)
		}
		if read.WorldID != worldID {
			t.Error("worldID should be the same")
		}
		if read.UserID != userID {
			t.Error("userID should be the same")
		}
		if !read.UpdatedAt.Equal(updatedAt.UTC().Truncate(time.Millisecond)) {
			t.Error("updatedAt should be the same")
		}
	}
}

func TestGetWorldLastLaunchByWorldID(t *testing.T) {

	scyllaClient := getClient()

	// generate data to be inserted
	worldID := uuid.New().String()
	userIDValues := []string{}
	updatedAtValues := []time.Time{}
	for i := 0; i < 10; i++ {
		userIDValues = append(userIDValues, uuid.New().String())
		updatedAtValues = append(updatedAtValues, time.Now())
		time.Sleep(time.Duration(100 * time.Millisecond))
	}

	// write new records
	{
		for i := range userIDValues {
			err := scyllaClient.UpsertWorldLastLaunch(WorldLastLaunch{
				WorldID:   worldID,
				UserID:    userIDValues[i],
				UpdatedAt: updatedAtValues[i],
			})
			if err != nil {
				t.Error(err)
			}
		}
	}

	// count all world last launches
	{
		count, err := scyllaClient.CountWorldLastLaunchByWorldID(worldID)
		if err != nil {
			t.Error(err)
		}
		if count != len(userIDValues) {
			t.Error("count should be the same")
		}
	}
}
