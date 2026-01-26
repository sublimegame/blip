package scyllaclient

// func TestTransactionDummy1(t *testing.T) {
// 	userID, err := uuid.Parse("00000000-0000-0000-9999-000000000001")
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	scyllaClient, err := New(ClientConfig{
// 		Servers:  []string{"172.17.0.2"},
// 		Keyspace: "universe",
// 	})
// 	if err != nil {
// 		t.Error(err)
// 	}
// 	err = scyllaClient.GrantUserCoin(userID, 15000000, "test1")
// 	if err != nil {
// 		t.Error(err)
// 	}
// }

// func TestTransactionDummy2(t *testing.T) {
// 	userID, err := uuid.Parse("00000000-0000-0000-9999-000000000001")
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	scyllaClient, err := New(ClientConfig{
// 		Servers:  []string{"172.17.0.2"},
// 		Keyspace: "universe",
// 	})
// 	if err != nil {
// 		t.Error(err)
// 	}
// 	balances, err := scyllaClient.GetUserBalances(userID.String())
// 	if err != nil {
// 		t.Error(err)
// 	}

// 	fmt.Println("balances:", balances)
// }

// func TestTransactionDummy(t *testing.T) {
// 	fmt.Println("🟢 INSERT INTO UniverseUserBalanceLockTable")
// 	stmts, names := transaction.UniverseUserBalanceLockTable.Insert()
// 	stmts += "IF NOT EXISTS USING TTL 1296000"
// 	fmt.Println("stmts:", stmts)
// 	fmt.Println("names:", names)
// }

// func TestTransactionDummy2(t *testing.T) {
// 	fmt.Println("🟢 INSERT INTO UniverseTransactionGroupTable")
// 	stmts, names := transaction.UniverseTransactionGroupTable.Insert()
// 	fmt.Println("stmts:", stmts)
// 	fmt.Println("names:", names)
// }

// func TestTransactionDummy3(t *testing.T) {
// 	fmt.Println("🟢 UPDATE UniverseUserBalanceTable")
// 	stmts, names := transaction.UniverseUserBalanceTable.UpdateBuilder().
// 		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS).
// 		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS).
// 		Where(qb.Eq(transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID)).ToCql()
// 	fmt.Println("stmts:", stmts)
// 	fmt.Println("names:", names)
// }

// func TestTransactionDummy4(t *testing.T) {
// 	fmt.Println("🟢 INSERT INTO UniverseGrantCoinTransactionByUserTable")
// 	stmts, names := transaction.UniverseGrantCoinTransactionByUserTable.Insert()
// 	fmt.Println("stmts:", stmts)
// 	fmt.Println("names:", names)
// }

// func TestTransactionDummy5(t *testing.T) {
// 	fmt.Println("🟢 INSERT INTO UniverseTreasuryTransactionGrantCoinTable")
// 	stmts, names := transaction.UniverseTreasuryTransactionGrantCoinTable.Insert()
// 	fmt.Println("stmts:", stmts)
// 	fmt.Println("names:", names)
// }
