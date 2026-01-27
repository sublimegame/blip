package scyllaclient

import (
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"
	"github.com/scylladb/gocqlx/v3/qb"

	"cu.bzh/hub/scyllaclient/model/notification"
)

// ----------------------------
// Read
// ----------------------------

type GetNotificationsOpts struct {
	Read     *bool
	Category *string
	Limit    uint
}

// Get notifications for a user
func (c *Client) GetUserNotifications(userID string, opts GetNotificationsOpts) ([]Notification, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// Build the query
	builder := notification.UniverseUserNotificationTable.SelectBuilder()

	// add conditions
	{
		cmps := make([]qb.Cmp, 0)
		if opts.Read != nil {
			cmps = append(cmps, qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_Read))
		}
		if opts.Category != nil {
			cmps = append(cmps, qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_Category))
		}
		if len(cmps) > 0 {
			builder = builder.Where(cmps...)
		}
		if len(cmps) > 1 {
			builder = builder.AllowFiltering()
		}
	}

	// add limit
	if opts.Limit > 0 {
		builder = builder.Limit(opts.Limit)
	}

	stmts, names := builder.ToCql()
	q := c.session.Query(stmts, names)

	// create and bind filter
	filter := qb.M{
		notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID: userID,
	}
	if opts.Read != nil {
		filter[notification.FIELD_UNIVERSE_USERNOTIFICATION_Read] = *opts.Read
	}
	if opts.Category != nil {
		filter[notification.FIELD_UNIVERSE_USERNOTIFICATION_Category] = *opts.Category
	}
	q = q.BindMap(filter)

	// Execute the query
	var records []notification.UniverseUserNotificationRecord
	err = q.SelectRelease(&records)
	if err != nil {
		return nil, err
	}

	notifications := make([]Notification, 0, len(records))
	for _, record := range records {
		// extract time from TimeUUID
		timeUUID, err := uuid.Parse(record.TimeUUID)
		if err != nil {
			// skip this record
			// TODO: report error!
			continue
		}
		// parse content JSON
		content, err := NewNotificationContent(record.Content)
		if err != nil {
			// skip this record
			// TODO: report error!
			continue
		}
		// create notification object
		notification := Notification{
			UserID:         record.UserID,
			NotificationID: record.TimeUUID,
			CreatedAt:      time.Unix(timeUUID.Time().UnixTime()),
			Read:           record.Read,
			Category:       content.Category,
			Title:          content.Title,
			Message:        content.Message,
		}
		notifications = append(notifications, notification)
	}

	return notifications, nil
}

func (c *Client) GetUserNotificationsUsers() (userIDs []string, err error) {
	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	stmts, names := notification.UniverseUserNotificationTable.SelectAll()
	fmt.Println("🐞[GetUserNotificationsUsers]", stmts, names)
	q := c.session.Query(stmts, names)
	iter := q.Iter()
	defer iter.Close()

	var record notification.UniverseUserNotificationRecord
	for {
		// var userID string
		if !iter.StructScan(&record) {
			fmt.Println("🐞[GetUserNotificationsUsers] SCAN ERROR")
			break
		}
		userIDs = append(userIDs, record.UserID)
	}
	fmt.Println("🐞[GetUserNotificationsUsers] result count:", len(userIDs))
	return
}

// ----------------------------
// Write
// ----------------------------

func (c *Client) DeleteUserNotification(userID, notificationID string) (err error) {
	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return
	}

	if notificationID == "" {
		err = errors.New("notification ID is empty")
		return
	}

	// Insert the record
	stmts, names := notification.UniverseUserNotificationTable.Delete()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID:   userID,
		notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID: notificationID,
	})
	err = q.ExecRelease()
	return
}

// CreateUserNotification creates an unread notification for a user
func (c *Client) CreateUserNotification(userID string, category, title, message string) (notificationID string, badgeCount uint, err error) {
	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return
	}

	if userID == "" {
		err = errors.New("user ID is empty")
		return
	}

	if category == "" {
		err = errors.New("category is empty")
		return
	}

	if title == "" {
		err = errors.New("title is empty")
		return
	}

	if message == "" {
		err = errors.New("message is empty")
		return
	}

	// create new UUID
	timeUUID, err := uuid.NewUUID()
	if err != nil {
		return
	}
	notificationID = timeUUID.String()

	contentJSON, err := notificationContent{
		Category: category,
		Title:    title,
		Message:  message,
	}.ToJSON()
	if err != nil {
		return
	}

	// Build the record
	record := notification.UniverseUserNotificationRecord{
		UserID:   userID,
		TimeUUID: notificationID,
		Category: category,
		Read:     false,
		Content:  string(contentJSON),
	}

	// Insert the record
	stmts, names := notification.UniverseUserNotificationTable.Insert()
	fmt.Println("🐞", stmts, names)
	q := c.session.Query(stmts, names)
	q = q.BindStruct(record)
	err = q.ExecRelease()
	if err != nil {
		return
	}

	// Get count of unread notifications
	builder := notification.UniverseUserNotificationTable.SelectBuilder()
	builder = builder.Count(notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID)
	builder = builder.Where(qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_Read))
	stmts, names = builder.ToCql()
	q = c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID: userID,
		notification.FIELD_UNIVERSE_USERNOTIFICATION_Read:   false,
	})
	var count int
	err = q.GetRelease(&count)
	if err != nil {
		return
	}
	badgeCount = uint(count)

	return
}

// MarkNotificationAsRead marks a notification as read for a user
func (c *Client) MarkNotificationAsRead(userID, notificationID string, newReadValue bool) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	if userID == "" {
		return errors.New("user ID is empty")
	}

	if notificationID == "" {
		return errors.New("notification ID is empty")
	}

	// Build the update query
	builder := notification.UniverseUserNotificationTable.UpdateBuilder()
	builder.Set(notification.FIELD_UNIVERSE_USERNOTIFICATION_Read)
	builder.Where(qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID), qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID))
	stmts, names := builder.ToCql()

	// Prepare the query
	q := c.session.Query(stmts, names)

	// Bind the parameters
	q = q.BindMap(qb.M{
		notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID:   userID,
		notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID: notificationID,
		notification.FIELD_UNIVERSE_USERNOTIFICATION_Read:     newReadValue,
	})

	// Execute the query
	err = q.ExecRelease()

	return err
}

func (c *Client) MarkNotificationsAsRead(userID string, filter NotificationFilter, newReadValue bool) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	// get all notifications to update
	notifications, err := c.GetUserNotifications(userID, GetNotificationsOpts{
		Read:     filter.Read,
		Category: filter.Category,
	})

	// Build the update query
	builder := notification.UniverseUserNotificationTable.UpdateBuilder()
	builder.Set(notification.FIELD_UNIVERSE_USERNOTIFICATION_Read)
	builder.Where(qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID), qb.Eq(notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID))
	stmts, names := builder.ToCql()

	// Prepare the query
	q := c.session.Query(stmts, names)

	for _, n := range notifications {
		// Bind the parameters
		q = q.BindMap(qb.M{
			notification.FIELD_UNIVERSE_USERNOTIFICATION_UserID:   n.UserID,
			notification.FIELD_UNIVERSE_USERNOTIFICATION_TimeUUID: n.NotificationID,
			notification.FIELD_UNIVERSE_USERNOTIFICATION_Read:     newReadValue,
		})
		execErr := q.Exec()
		if execErr != nil {
			err = execErr
			continue
		}
	}
	q.Release()

	return err
}

func (c *Client) RemoveAllNotificationsForUser(userID string) (count int, err error) {
	err = c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return 0, err
	}

	// get all notifications for the user
	allNotifications, err := c.GetUserNotifications(userID, GetNotificationsOpts{})
	if err != nil {
		return 0, err
	}

	// delete each notification
	count = 0
	for _, n := range allNotifications {
		err := c.DeleteUserNotification(userID, n.NotificationID)
		if err != nil {
			return count, err
		}
		count++
	}

	return count, nil
}

// ----------------------------
// Types
// ----------------------------

type Notification struct {
	UserID         string    `json:"userID"`
	NotificationID string    `json:"notificationID"`
	CreatedAt      time.Time `json:"created"`
	Read           bool      `json:"read"`
	Category       string    `json:"category"`
	Title          string    `json:"title"`
	Message        string    `json:"message"`
}

type NotificationFilter struct {
	Read     *bool
	Category *string
}

type notificationContent struct {
	Category string `json:"category"`
	Title    string `json:"title"`
	Message  string `json:"message"`
}

func NewNotificationContent(jsonStr string) (notificationContent, error) {
	var n notificationContent
	err := json.Unmarshal([]byte(jsonStr), &n)
	if err != nil {
		return n, err
	}
	return n, nil
}

func (n notificationContent) ToJSON() (string, error) {
	jsonBytes, err := json.Marshal(n)
	if err != nil {
		return "", err
	}
	return string(jsonBytes), nil
}
