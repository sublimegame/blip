package scyllaclient

// func TestLeaderboardScoreGetAround(t *testing.T) {

// 	scyllaClient, err := New(ClientConfig{
// 		Servers:  []string{"172.17.0.4"},
// 		Keyspace: "kvs",
// 	})
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	records, err := scyllaClient.GetLeaderboardScoresAround("00000000-0000-0000-0000-000000000001", "default", 30, 5)
// 	if err != nil {
// 		fmt.Println("❌ failed to insert or update record")
// 		t.Error(err)
// 	} else {
// 		fmt.Println("✅ Got records:")
// 		for _, rec := range records {
// 			fmt.Println("->", rec)
// 		}
// 	}
// }

// func TestLeaderboardScoreGetAround2(t *testing.T) {
// 	scyllaClient, err := New(ClientConfig{
// 		Servers:  []string{"172.17.0.4"},
// 		Keyspace: "kvs",
// 	})
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	record, err := scyllaClient.GetLeaderboardScoreForUser("00000000-0000-0000-0000-000000000001", "default", "00000000-0000-0000-0000-000000009999")
// 	fmt.Println("record:", record)
// 	fmt.Println("err:", err)
// }
