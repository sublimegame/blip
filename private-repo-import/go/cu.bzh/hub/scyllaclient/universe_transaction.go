package scyllaclient

import (
	"encoding/json"
	"errors"
	"sort"
	"time"

	"github.com/gocql/gocql"
	"github.com/google/uuid"
	"github.com/scylladb/gocqlx/v3/qb"

	"cu.bzh/hub/scyllaclient/model/transaction"
)

// ----------------------------
// Read
// ----------------------------

// Get balances for a user
func (c *Client) GetUserBalances(userID string) (*UserBalance, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	balance := &UserBalance{
		UserID:         userID,
		DummyCoins:     0,
		GrantedCoins:   0,
		EarnedCoins:    0,
		PurchasedCoins: 0,
		TotalCoins:     0,
	}

	stmts, names := transaction.UniverseUserBalanceTable.Get()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID: userID,
	})
	var record transaction.UniverseUserBalanceRecord
	err = q.GetRelease(&record)
	if err == gocql.ErrNotFound {
		return balance, nil // success (no balance found)
	} else if err != nil {
		return nil, err
	}

	balance.DummyCoins = record.TotalDummyCoins
	balance.GrantedCoins = record.TotalGrantedCoins
	balance.EarnedCoins = record.TotalEarnedCoins
	balance.PurchasedCoins = record.TotalPurchasedCoins
	balance.TotalCoins = record.TotalCoins

	return balance, nil
}

// Get transactions for a user
func (c *Client) GetUserTransactions(userID string, limit uint) ([]*TransactionResponse, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	transactions := make([]*TransactionResponse, 0)

	// GRANTS

	stmts, names := transaction.UniverseGrantCoinTransactionByUserTable.SelectBuilder().Limit(limit).ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID: userID,
	})
	var grantRecords []transaction.UniverseGrantCoinTransactionByUserRecord
	err = q.SelectRelease(&grantRecords)
	if err != nil {
		return nil, err
	}

	for _, record := range grantRecords {
		createdAt := time.Unix(record.CreatedAt/1000, (record.CreatedAt%1000)*1000000)
		var infoObj TransactionInfo
		err := json.Unmarshal([]byte(record.Info), &infoObj)
		if err != nil {
			return nil, err
		}
		transactions = append(transactions, &TransactionResponse{
			UserID:        record.UserID,
			TransactionID: record.TransactionID,
			CreatedAt:     createdAt,
			Amount:        record.Amount / 1000000, // convert from millionths to units
			Info:          infoObj,
		})
	}

	// PURCHASES

	stmts, names = transaction.UniversePurchaseCoinTransactionByUserTable.SelectBuilder().Limit(limit).ToCql()
	q = c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_PurchaseCoinTransactionByUser_USER_ID: userID,
	})
	var purchaseRecords []transaction.UniversePurchaseCoinTransactionByUserRecord
	err = q.SelectRelease(&purchaseRecords)
	if err != nil {
		return nil, err
	}

	for _, record := range purchaseRecords {
		createdAt := time.Unix(record.CreatedAt/1000, (record.CreatedAt%1000)*1000000)
		var infoObj TransactionInfo
		err := json.Unmarshal([]byte(record.Info), &infoObj)
		if err != nil {
			return nil, err
		}
		transactions = append(transactions, &TransactionResponse{
			UserID:        record.UserID,
			TransactionID: record.TransactionID,
			CreatedAt:     createdAt,
			Amount:        record.Amount / 1000000, // convert from millionths to units
			Info:          infoObj,
		})
	}

	// Sort transactions by createdAt in descending order (newest first)
	sort.Slice(transactions, func(i, j int) bool {
		return transactions[i].CreatedAt.After(transactions[j].CreatedAt)
	})

	return transactions, nil
}

// UserHasCoinGrantForReason ...
func (c *Client) UserHasCoinGrantForReason(userID string, reason string) (bool, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return false, err
	}

	found := false

	// List all "grant coin" transactions for the user
	// (only the INFO field is needed)
	stmts, names := transaction.UniverseGrantCoinTransactionByUserTable.Select(transaction.FIELD_UNIVERSE_GrantCoinTransactionByUser_INFO)
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID: userID,
	})
	iter := q.Iter()
	defer iter.Close()

	var record transaction.UniverseGrantCoinTransactionByUserRecord
	var transactionInfo TransactionInfo
	for {
		if !iter.StructScan(&record) {
			break
		}
		// Parse the INFO field
		transactionInfo, err = NewTransactionInfo(record.Info)
		if err != nil {
			return false, err
		}
		// Check if the reason matches
		if transactionInfo.Reason == reason {
			found = true
			break
		}
	}

	return found, nil
}

// UserReceivedPurchasedCoinsForReason ...
func (c *Client) UserReceivedPurchasedCoinsForReason(userID string, reason string) (bool, error) {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return false, err
	}

	found := false

	// List all "purchased coins" transactions for the user
	// (only the INFO field is needed)
	stmts, names := transaction.UniversePurchaseCoinTransactionByUserTable.Select(transaction.FIELD_UNIVERSE_PurchaseCoinTransactionByUser_INFO)
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_PurchaseCoinTransactionByUser_USER_ID: userID,
	})
	iter := q.Iter()
	defer iter.Close()

	var record transaction.UniversePurchaseCoinTransactionByUserRecord
	var transactionInfo TransactionInfo
	for {
		if !iter.StructScan(&record) {
			break
		}
		// Parse the INFO field
		transactionInfo, err = NewTransactionInfo(record.Info)
		if err != nil {
			return false, err
		}
		// Check if the reason matches
		if transactionInfo.Reason == reason {
			found = true
			break
		}
	}

	return found, nil
}

// ----------------------------
// Write
// ----------------------------

// RebuildUserBalanceCache rebuilds the balance cache for a user from transaction history.
// This is useful when the cache becomes out of sync with the actual transaction data.
func (c *Client) RebuildUserBalanceCache(userID string) error {
	balancesCache, err := c.GetUserBalances(userID)
	if err != nil {
		return err
	}

	if balancesCache == nil {
		// no error, just no balance cache for this user (no transactions yet)
		return nil
	}

	balances, err := c.GetUserBalancesComputedFromTransactions(userID)
	if err != nil {
		return err
	}

	updateBuilder := transaction.UniverseUserBalanceTable.UpdateBuilder()
	updateBuilder.
		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_DUMMY_COINS).
		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS).
		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS).
		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS).
		Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS)
	stmts, names := updateBuilder.Where(qb.Eq(transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID)).ToCql()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID:               userID,
		transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_DUMMY_COINS:     balances.DummyCoins - balancesCache.DummyCoins,
		transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS:   balances.GrantedCoins - balancesCache.GrantedCoins,
		transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS: balances.PurchasedCoins - balancesCache.PurchasedCoins,
		transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS:    balances.EarnedCoins - balancesCache.EarnedCoins,
		transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS:           balances.TotalCoins - balancesCache.TotalCoins,
	})
	err = q.ExecRelease()
	return err
}

// GrantUserCoin grants coins to a user.
// The amount is in millionths of coins.
// The reason is a short string describing the reason for the grant (e.g. "daily_bonus"). It is mandatory.
// The description is a longer string that can be displayed to users. It is optional.
func (c *Client) GrantUserCoin(userID string, amount int64, reason, description string) error {

	if userID == "" {
		return errors.New("user ID is empty")
	}

	if amount == 0 {
		return errors.New("amount is zero")
	}

	if reason == "" {
		return errors.New("reason is empty")
	}

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	now := time.Now()

	// acquire lock
	err = c.acquireUserBalanceLock(userID, now)
	if err != nil {
		return err
	}
	// release lock on return
	defer c.releaseUserBalanceLock(userID)

	// create transactions elements (batch)
	{
		// Create CQL batch
		batch := c.session.NewBatch(gocql.LoggedBatch)

		// insert transaction group
		// prepare transaction info object
		trGroupInfoJsonStr, err := TransactionGroupInfo{
			Type:        TRANSACTION_TYPE_GRANT_COIN,
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}
		stmts, _ := transaction.UniverseTransactionGroupTable.Insert()
		transactionGroupID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, transactionGroupID, now, trGroupInfoJsonStr)

		// insert user transaction
		trInfoJsonStr, err := TransactionInfo{
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}
		stmts, _ = transaction.UniverseGrantCoinTransactionByUserTable.Insert()
		userTransactionID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, userID, now, userTransactionID, transactionGroupID, amount, trInfoJsonStr)

		// insert treasury transaction
		stmts, _ = transaction.UniverseTreasuryTransactionGrantCoinTable.Insert()
		treasuryTransactionID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, transactionGroupID, treasuryTransactionID, -amount)

		err = c.session.ExecuteBatch(batch)
		if err != nil {
			return err
		}
	}

	// update user balance
	{
		updateBuilder := transaction.UniverseUserBalanceTable.UpdateBuilder()
		updateBuilder.
			Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS).
			Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS)
		stmts, names := updateBuilder.Where(qb.Eq(transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID)).ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(qb.M{
			transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID:             userID,
			transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS: amount,
			transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS:         amount,
		})
		err = q.ExecRelease()
		if err != nil {
			return err
		}
	}

	return nil // success
}

// UserMakesPaymentForReason debits coins from a user.
// The amount is in millionths of coins.
// The reason is a short string describing the reason for the debit (e.g. "payment for <foo>"). It is mandatory.
// The description is a longer string that can be displayed to users. It is optional.
func (c *Client) UserMakesPaymentForReason(userID string,
	grantedCoinAmountMillionth,
	purchasedCoinAmountMillionth,
	earnedCoinAmountMillionth int64,
	reason, description string) error {

	if userID == "" {
		return errors.New("user ID is empty")
	}

	if grantedCoinAmountMillionth < 0 {
		return errors.New("granted coin amount is negative")
	}

	if purchasedCoinAmountMillionth < 0 {
		return errors.New("purchased coin amount is negative")
	}

	if earnedCoinAmountMillionth < 0 {
		return errors.New("earned coin amount is negative")
	}

	if grantedCoinAmountMillionth == 0 && purchasedCoinAmountMillionth == 0 && earnedCoinAmountMillionth == 0 {
		return errors.New("amount is zero")
	}

	if reason == "" {
		return errors.New("reason is empty")
	}

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	now := time.Now()

	// acquire lock
	err = c.acquireUserBalanceLock(userID, now)
	if err != nil {
		return err
	}
	// release lock on return
	defer c.releaseUserBalanceLock(userID)

	// create transactions elements (batch)
	{
		// Create CQL batch
		batch := c.session.NewBatch(gocql.LoggedBatch)

		// insert transaction group
		// prepare transaction info object
		trGroupInfoJsonStr, err := TransactionGroupInfo{
			Type:        TRANSACTION_TYPE_DEBIT_COINS,
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}
		stmts, _ := transaction.UniverseTransactionGroupTable.Insert()
		transactionGroupID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, transactionGroupID, now, trGroupInfoJsonStr)

		// insert user transactions (1 per coin type)

		trInfoJsonStr, err := TransactionInfo{
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}

		if grantedCoinAmountMillionth > 0 {
			{ // granted coins (user)
				stmts, _ = transaction.UniverseGrantCoinTransactionByUserTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, userID, now, transactionID, transactionGroupID, -grantedCoinAmountMillionth, trInfoJsonStr)
			}
			{ // granted coins (treasury)
				stmts, _ = transaction.UniverseTreasuryTransactionGrantCoinTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, transactionGroupID, transactionID, grantedCoinAmountMillionth)
			}
		}

		if purchasedCoinAmountMillionth > 0 {
			{ // purchased coins (user)
				stmts, _ = transaction.UniversePurchaseCoinTransactionByUserTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, userID, now, transactionID, transactionGroupID, -purchasedCoinAmountMillionth, trInfoJsonStr)
			}
			{ // purchased coins (treasury)
				stmts, _ = transaction.UniverseTreasuryTransactionPurchaseCoinTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, transactionGroupID, transactionID, purchasedCoinAmountMillionth)
			}
		}

		if earnedCoinAmountMillionth > 0 {
			{ // earned coins (user)
				stmts, _ = transaction.UniverseEarnCoinTransactionByUserTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, userID, now, transactionID, transactionGroupID, -earnedCoinAmountMillionth, trInfoJsonStr)
			}
			{ // earned coins (treasury)
				stmts, _ = transaction.UniverseTreasuryTransactionEarnCoinTable.Insert()
				transactionID, err := newUUIDString()
				if err != nil {
					return err
				}
				batch.Query(stmts, transactionGroupID, transactionID, earnedCoinAmountMillionth)
			}
		}

		err = c.session.ExecuteBatch(batch)
		if err != nil {
			return err
		}
	}

	// update user balance
	// by adding the amounts of the transactions to the user balance
	{
		updateBuilder := transaction.UniverseUserBalanceTable.UpdateBuilder()
		updateData := qb.M{
			transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID: userID,
		}

		if grantedCoinAmountMillionth > 0 {
			updateBuilder.Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS)
			updateData[transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS] = -1 * grantedCoinAmountMillionth
		}

		if purchasedCoinAmountMillionth > 0 {
			updateBuilder.Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS)
			updateData[transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS] = -1 * purchasedCoinAmountMillionth
		}

		if earnedCoinAmountMillionth > 0 {
			updateBuilder.Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS)
			updateData[transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS] = -1 * earnedCoinAmountMillionth
		}

		updateBuilder.Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS)
		updateData[transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS] = -1 * (grantedCoinAmountMillionth + purchasedCoinAmountMillionth + earnedCoinAmountMillionth)

		stmts, names := updateBuilder.Where(qb.Eq(transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID)).ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(updateData)
		err = q.ExecRelease()
		if err != nil {
			return err
		}
	}

	return nil // success
}

// CreditUserPurchasedCoins credits purchased coins to a user.
// The amount is in millionths of coins.
// The reason is usually a transaction ID, prefixed by platform name (e.g. "apple-iap_transaction_1234567890").
// The description is a longer string that can be displayed to users. It is optional.
func (c *Client) CreditUserPurchasedCoins(userID string, amount int64, reason, description string) error {

	if userID == "" {
		return errors.New("user ID is empty")
	}

	if amount == 0 {
		return errors.New("amount is zero")
	}

	if reason == "" {
		return errors.New("reason is empty")
	}

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}

	now := time.Now()

	// acquire lock
	err = c.acquireUserBalanceLock(userID, now)
	if err != nil {
		return err
	}
	// release lock on return
	defer c.releaseUserBalanceLock(userID)

	// create transactions elements (batch)
	{
		// Create CQL batch
		batch := c.session.NewBatch(gocql.LoggedBatch)

		// insert transaction group
		// prepare transaction info object
		trGroupInfoJsonStr, err := TransactionGroupInfo{
			Type:        TRANSACTION_TYPE_PURCHASED_COINS,
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}
		stmts, _ := transaction.UniverseTransactionGroupTable.Insert()
		transactionGroupID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, transactionGroupID, now, trGroupInfoJsonStr)

		// insert user transaction
		trInfoJsonStr, err := TransactionInfo{
			Reason:      reason,
			Description: description,
		}.ToJsonString()
		if err != nil {
			return err
		}
		stmts, _ = transaction.UniversePurchaseCoinTransactionByUserTable.Insert()
		userTransactionID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, userID, now, userTransactionID, transactionGroupID, amount, trInfoJsonStr)

		// insert treasury transaction
		stmts, _ = transaction.UniverseTreasuryTransactionPurchaseCoinTable.Insert()
		treasuryTransactionID, err := newUUIDString()
		if err != nil {
			return err
		}
		batch.Query(stmts, transactionGroupID, treasuryTransactionID, -amount)

		err = c.session.ExecuteBatch(batch)
		if err != nil {
			return err
		}
	}

	// update user balance
	{
		updateBuilder := transaction.UniverseUserBalanceTable.UpdateBuilder()
		updateBuilder.
			Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS).
			Add(transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS)
		stmts, names := updateBuilder.Where(qb.Eq(transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID)).ToCql()
		q := c.session.Query(stmts, names)
		q = q.BindMap(qb.M{
			transaction.FIELD_UNIVERSE_USERBALANCE_USER_ID:               userID,
			transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS: amount,
			transaction.FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS:           amount,
		})
		err = q.ExecRelease()
		if err != nil {
			return err
		}
	}

	return nil // success
}

func (c *Client) GetUserBalancesComputedFromTransactions(userID string) (*UserBalance, error) {
	var resultBalances *UserBalance = nil

	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return nil, err
	}

	// Coins granted
	{
		stmts, names := transaction.UniverseGrantCoinTransactionByUserTable.Select()
		q := c.session.Query(stmts, names)
		q = q.BindMap(qb.M{
			transaction.FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID: userID,
		})
		iter := q.Iter()
		defer iter.Close()

		var record transaction.UniverseGrantCoinTransactionByUserRecord
		for {
			if !iter.StructScan(&record) {
				break
			}
			if resultBalances == nil {
				resultBalances = &UserBalance{}
			}
			resultBalances.GrantedCoins += record.Amount
			resultBalances.TotalCoins += record.Amount
		}
	}

	// TODO: consider other coin transaction types

	return resultBalances, nil
}

// Types

type UserBalance struct {
	UserID         string
	DummyCoins     int64
	GrantedCoins   int64
	EarnedCoins    int64
	PurchasedCoins int64
	TotalCoins     int64
}

// TransactionInfo

type TransactionInfo struct {
	Reason      string `json:"reason"`
	Description string `json:"description,omitempty"`
}

func NewTransactionInfo(jsonString string) (TransactionInfo, error) {
	var ti TransactionInfo
	err := json.Unmarshal([]byte(jsonString), &ti)
	return ti, err
}

func (ti TransactionInfo) ToJsonString() (string, error) {
	jsonBytes, err := json.Marshal(ti)
	if err != nil {
		return "", err
	}
	return string(jsonBytes), nil
}

// TransactionGroupInfo

type TransactionGroupInfo struct {
	Type        string `json:"type"`
	Reason      string `json:"reason"`
	Description string `json:"description,omitempty"`
}

func NewTransactionGroupInfo(jsonString string) (TransactionGroupInfo, error) {
	var tgi TransactionGroupInfo
	err := json.Unmarshal([]byte(jsonString), &tgi)
	return tgi, err
}

func (tgi TransactionGroupInfo) ToJsonString() (string, error) {
	jsonBytes, err := json.Marshal(tgi)
	if err != nil {
		return "", err
	}
	return string(jsonBytes), nil
}

type TransactionResponse struct {
	Type          string          `json:"type,omitempty"`
	UserID        string          `json:"user_id"`
	TransactionID string          `json:"transaction_id"`
	CreatedAt     time.Time       `json:"created_at"`
	Amount        int64           `json:"amount"`
	Info          TransactionInfo `json:"info"`
}

const (
	TRANSACTION_TYPE_GRANT_COIN      = "grant_coin"
	TRANSACTION_TYPE_PURCHASED_COINS = "purchased_coins"
	TRANSACTION_TYPE_DEBIT_COINS     = "debit_coins"
)

// Utility functions

func newUUIDString() (string, error) {
	uuidObj, err := uuid.NewUUID()
	if err != nil {
		return "", err
	}
	return uuidObj.String(), nil
}

func (c *Client) acquireUserBalanceLock(userID string, now time.Time) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}
	stmts, names := transaction.UniverseUserBalanceLockTable.Insert()
	stmts += "IF NOT EXISTS USING TTL 1296000"
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_USERBALANCELOCK_USER_ID:    userID,
		transaction.FIELD_UNIVERSE_USERBALANCELOCK_CREATED_AT: now,
	})
	applied, err := q.ExecCASRelease() // execute lightweight transaction
	if err != nil {
		return err
	}
	if !applied {
		return errors.New("could now acquire lock")
	}
	return nil
}

func (c *Client) releaseUserBalanceLock(userID string) error {
	err := c.ensureClientKeyspace(keyspaceUniverse)
	if err != nil {
		return err
	}
	stmts, names := transaction.UniverseUserBalanceLockTable.Delete()
	q := c.session.Query(stmts, names)
	q = q.BindMap(qb.M{
		transaction.FIELD_UNIVERSE_USERBALANCELOCK_USER_ID: userID,
	})
	err = q.ExecRelease()
	return err
}
