package scyllaclient

// func TestTransactionDummy1(t *testing.T) {
// 	userID, err := uuid.Parse("00000000-0000-0000-9999-000000000001")
// 	if err != nil {
// 		t.Error(err)
// 	}
// 	fmt.Println(userID)
// }

// func TestTransactionDummy2(t *testing.T) {
// 	timeUUID, err := uuid.NewUUID()
// 	if err != nil {
// 		t.Error(err)
// 	}
// 	fmt.Println("TimeUUID:", timeUUID)
// 	fmt.Println("Time:", time.Unix(timeUUID.Time().UnixTime()))
// }

// var notificationID string = ""

// func TestNotificationInsert(t *testing.T) {
// 	userID := "00000000-0000-0000-0000-000000000001"

// 	scyllaClient, err := New(ClientConfig{
// 		Servers:  []string{"172.17.0.3"},
// 		Keyspace: "universe",
// 	})
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	notificationID, err = scyllaClient.CreateUserNotification(userID, "test", "title", "message")
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	err = scyllaClient.MarkNotificationAsRead(userID, notificationID)
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	fmt.Println(">>>> GET ALL")
// 	notifs, err := scyllaClient.GetUserNotifications(userID, GetNotificationsOpts{})
// 	if err != nil {
// 		t.Error(err)
// 	}
// 	fmt.Println("     COUNT:", len(notifs))
// 	if len(notifs) < 1 {
// 		t.Error("Expected 1+ notification")
// 	}
// 	for _, n := range notifs {
// 		fmt.Println("➡️", n.NotificationID, n.Title, n.Message, n.CreatedAt)
// 	}

// 	fmt.Println(">>>> GET ALL (read)")
// 	{
// 		trueValue := true
// 		notifs, err = scyllaClient.GetUserNotifications(userID, GetNotificationsOpts{
// 			Read: &trueValue,
// 		})
// 		fmt.Println("     COUNT:", len(notifs))
// 		if err != nil {
// 			t.Error(err)
// 		}
// 		if len(notifs) < 1 {
// 			t.Error("Expected 1+ notification")
// 		}
// 		for _, n := range notifs {
// 			fmt.Println("➡️", n.NotificationID, n.Title, n.Message, n.CreatedAt)
// 		}
// 	}

// 	fmt.Println(">>>> GET ALL (unread)")
// 	{
// 		falseValue := false
// 		notifs, err = scyllaClient.GetUserNotifications(userID, GetNotificationsOpts{
// 			Read: &falseValue,
// 		})
// 		fmt.Println("     COUNT:", len(notifs))
// 		if err != nil {
// 			t.Error(err)
// 		}
// 		if len(notifs) < 1 {
// 			t.Error("Expected 1+ notification")
// 		}
// 		for _, n := range notifs {
// 			fmt.Println("➡️", n.NotificationID, n.Title, n.Message, n.CreatedAt)
// 		}
// 	}
// }
