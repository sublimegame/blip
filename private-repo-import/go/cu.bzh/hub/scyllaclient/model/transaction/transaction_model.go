package transaction

import (
	"github.com/scylladb/gocqlx/v3/table"
)

// ----------------------------------------------
// Table : universe.user_balance
// ----------------------------------------------

// CREATE TABLE universe.user_balance (
//    user_id UUID,
//    total_dummy_coins COUNTER,     -- coins received as grant by users that are not confirmed yet
//    total_granted_coins COUNTER,
//    total_earned_coins COUNTER,
//    total_purchased_coins COUNTER,
//    total_coins COUNTER,
//    PRIMARY KEY ((user_id))
// );

const (
	TABLE_UNIVERSE_USERBALANCE                       = "user_balance"
	FIELD_UNIVERSE_USERBALANCE_USER_ID               = "user_id"
	FIELD_UNIVERSE_USERBALANCE_TOTAL_DUMMY_COINS     = "total_dummy_coins"
	FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS   = "total_granted_coins"
	FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS    = "total_earned_coins"
	FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS = "total_purchased_coins"
	FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS           = "total_coins"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeUserBalanceTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_USERBALANCE,
	Columns: []string{
		FIELD_UNIVERSE_USERBALANCE_USER_ID,
		FIELD_UNIVERSE_USERBALANCE_TOTAL_DUMMY_COINS,
		FIELD_UNIVERSE_USERBALANCE_TOTAL_GRANTED_COINS,
		FIELD_UNIVERSE_USERBALANCE_TOTAL_EARNED_COINS,
		FIELD_UNIVERSE_USERBALANCE_TOTAL_PURCHASED_COINS,
		FIELD_UNIVERSE_USERBALANCE_TOTAL_COINS,
	},
	PartKey: []string{FIELD_UNIVERSE_USERBALANCE_USER_ID},
	SortKey: []string{},
}

var UniverseUserBalanceTable = table.New(universeUserBalanceTableMetadata)

type UniverseUserBalanceRecord struct {
	UserID              string
	TotalDummyCoins     int64
	TotalGrantedCoins   int64
	TotalEarnedCoins    int64
	TotalPurchasedCoins int64
	TotalCoins          int64
}

func (prr UniverseUserBalanceRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.TotalDummyCoins,
		&prr.TotalGrantedCoins,
		&prr.TotalEarnedCoins,
		&prr.TotalPurchasedCoins,
		&prr.TotalCoins,
	}
}

// ----------------------------------------------
// Table : universe.user_balance_lock
// ----------------------------------------------

// CREATE TABLE universe.user_balance_lock (
//    user_id UUID,
//    created_at TIMESTAMP,
//    PRIMARY KEY ((user_id))
// );

const (
	TABLE_UNIVERSE_USERBALANCELOCK            = "user_balance_lock"
	FIELD_UNIVERSE_USERBALANCELOCK_USER_ID    = "user_id"
	FIELD_UNIVERSE_USERBALANCELOCK_CREATED_AT = "created_at"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeUserBalanceLockTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_USERBALANCELOCK,
	Columns: []string{
		FIELD_UNIVERSE_USERBALANCELOCK_USER_ID,
		FIELD_UNIVERSE_USERBALANCELOCK_CREATED_AT,
	},
	PartKey: []string{FIELD_UNIVERSE_USERBALANCELOCK_USER_ID},
	SortKey: []string{},
}

var UniverseUserBalanceLockTable = table.New(universeUserBalanceLockTableMetadata)

type UniverseUserBalanceLockRecord struct {
	UserID    string
	CreatedAt int64
}

func (prr UniverseUserBalanceLockRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.CreatedAt,
	}
}

// ----------------------------------------------
// Table : universe.transaction_group
// ----------------------------------------------

// CREATE TABLE universe.transaction_group (
//    transaction_group_id UUID,
//    created_at TIMESTAMP,
//    info TEXT,
//    PRIMARY KEY ((transaction_group_id))
// );

const (
	TABLE_UNIVERSE_TRANSACTIONGROUP            = "transaction_group"
	FIELD_UNIVERSE_TRANSACTIONGROUP_ID         = "transaction_group_id"
	FIELD_UNIVERSE_TRANSACTIONGROUP_CREATED_AT = "created_at"
	FIELD_UNIVERSE_TRANSACTIONGROUP_INFO       = "info"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeTransactionGroupTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_TRANSACTIONGROUP,
	Columns: []string{
		FIELD_UNIVERSE_TRANSACTIONGROUP_ID,
		FIELD_UNIVERSE_TRANSACTIONGROUP_CREATED_AT,
		FIELD_UNIVERSE_TRANSACTIONGROUP_INFO,
	},
	PartKey: []string{FIELD_UNIVERSE_TRANSACTIONGROUP_ID},
	SortKey: []string{},
}

var UniverseTransactionGroupTable = table.New(universeTransactionGroupTableMetadata)

type UniverseTransactionGroupRecord struct {
	TransactionGroupID string
	CreatedAt          int64
	Info               string
}

func (prr UniverseTransactionGroupRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.TransactionGroupID,
		&prr.CreatedAt,
		&prr.Info,
	}
}

// ----------------------------------------------
// Table : universe.grant_coin_transaction_by_user
// ----------------------------------------------

// CREATE TABLE universe.grant_coin_transaction_by_user (
//    user_id UUID,
//    created_at TIMESTAMP,
//    transaction_id UUID,
//    transaction_group_id UUID,
//    amount BIGINT,
//    info TEXT,
//    PRIMARY KEY ((user_id), created_at, transaction_id)
// ) WITH CLUSTERING ORDER BY (created_at DESC);

const (
	TABLE_UNIVERSE_GrantCoinTransactionByUser                      = "grant_coin_transaction_by_user"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID              = "user_id"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_CREATED_AT           = "created_at"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_AMOUNT               = "amount"
	FIELD_UNIVERSE_GrantCoinTransactionByUser_INFO                 = "info"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeGrantCoinTransactionByUserTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_GrantCoinTransactionByUser,
	Columns: []string{
		FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID,
		FIELD_UNIVERSE_GrantCoinTransactionByUser_CREATED_AT,
		FIELD_UNIVERSE_GrantCoinTransactionByUser_TRANSACTION_ID,
		FIELD_UNIVERSE_GrantCoinTransactionByUser_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_GrantCoinTransactionByUser_AMOUNT,
		FIELD_UNIVERSE_GrantCoinTransactionByUser_INFO,
	},
	PartKey: []string{FIELD_UNIVERSE_GrantCoinTransactionByUser_USER_ID},
	SortKey: []string{FIELD_UNIVERSE_GrantCoinTransactionByUser_CREATED_AT, FIELD_UNIVERSE_GrantCoinTransactionByUser_TRANSACTION_ID},
}

var UniverseGrantCoinTransactionByUserTable = table.New(universeGrantCoinTransactionByUserTableMetadata)

type UniverseGrantCoinTransactionByUserRecord struct {
	UserID             string
	CreatedAt          int64
	TransactionID      string
	TransactionGroupID string
	Amount             int64
	Info               string
}

func (prr UniverseGrantCoinTransactionByUserRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.CreatedAt,
		&prr.TransactionID,
		&prr.TransactionGroupID,
		&prr.Amount,
		&prr.Info,
	}
}

// ----------------------------------------------
// Table : universe.dummy_coin_transaction_by_user
// ----------------------------------------------

// CREATE TABLE universe.dummy_coin_transaction_by_user (
//    user_id UUID,
//    created_at TIMESTAMP,
//    transaction_id UUID,
//    transaction_group_id UUID,
//    amount BIGINT,
//    info TEXT,
//    PRIMARY KEY ((user_id), created_at, transaction_id)
// ) WITH CLUSTERING ORDER BY (created_at DESC);

const (
	TABLE_UNIVERSE_DummyCoinTransactionByUser                      = "dummy_coin_transaction_by_user"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_USER_ID              = "user_id"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_CREATED_AT           = "created_at"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_AMOUNT               = "amount"
	FIELD_UNIVERSE_DummyCoinTransactionByUser_INFO                 = "info"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeDummyCoinTransactionByUserTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_DummyCoinTransactionByUser,
	Columns: []string{
		FIELD_UNIVERSE_DummyCoinTransactionByUser_USER_ID,
		FIELD_UNIVERSE_DummyCoinTransactionByUser_CREATED_AT,
		FIELD_UNIVERSE_DummyCoinTransactionByUser_TRANSACTION_ID,
		FIELD_UNIVERSE_DummyCoinTransactionByUser_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_DummyCoinTransactionByUser_AMOUNT,
		FIELD_UNIVERSE_DummyCoinTransactionByUser_INFO,
	},
	PartKey: []string{FIELD_UNIVERSE_DummyCoinTransactionByUser_USER_ID},
	SortKey: []string{FIELD_UNIVERSE_DummyCoinTransactionByUser_CREATED_AT, FIELD_UNIVERSE_DummyCoinTransactionByUser_TRANSACTION_ID},
}

var UniverseDummyCoinTransactionByUserTable = table.New(universeDummyCoinTransactionByUserTableMetadata)

type UniverseDummyCoinTransactionByUserRecord struct {
	UserID             string
	CreatedAt          int64
	TransactionID      string
	TransactionGroupID string
	Amount             int64
	Info               string
}

func (prr UniverseDummyCoinTransactionByUserRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.CreatedAt,
		&prr.TransactionID,
		&prr.TransactionGroupID,
		&prr.Amount,
		&prr.Info,
	}
}

// ----------------------------------------------
// Table : universe.earn_coin_transaction_by_user
// ----------------------------------------------

// CREATE TABLE universe.earn_coin_transaction_by_user (
//    user_id UUID,
//    created_at TIMESTAMP,
//    transaction_id UUID,
//    transaction_group_id UUID,
//    amount BIGINT,
//    info TEXT,
//    PRIMARY KEY ((user_id), created_at, transaction_id)
// ) WITH CLUSTERING ORDER BY (created_at DESC);

const (
	TABLE_UNIVERSE_EarnCoinTransactionByUser                      = "earn_coin_transaction_by_user"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_USER_ID              = "user_id"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_CREATED_AT           = "created_at"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_AMOUNT               = "amount"
	FIELD_UNIVERSE_EarnCoinTransactionByUser_INFO                 = "info"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeEarnCoinTransactionByUserTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_EarnCoinTransactionByUser,
	Columns: []string{
		FIELD_UNIVERSE_EarnCoinTransactionByUser_USER_ID,
		FIELD_UNIVERSE_EarnCoinTransactionByUser_CREATED_AT,
		FIELD_UNIVERSE_EarnCoinTransactionByUser_TRANSACTION_ID,
		FIELD_UNIVERSE_EarnCoinTransactionByUser_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_EarnCoinTransactionByUser_AMOUNT,
		FIELD_UNIVERSE_EarnCoinTransactionByUser_INFO,
	},
	PartKey: []string{FIELD_UNIVERSE_EarnCoinTransactionByUser_USER_ID},
	SortKey: []string{FIELD_UNIVERSE_EarnCoinTransactionByUser_CREATED_AT, FIELD_UNIVERSE_EarnCoinTransactionByUser_TRANSACTION_ID},
}

var UniverseEarnCoinTransactionByUserTable = table.New(universeEarnCoinTransactionByUserTableMetadata)

type UniverseEarnCoinTransactionByUserRecord struct {
	UserID             string
	CreatedAt          int64
	TransactionID      string
	TransactionGroupID string
	Amount             int64
	Info               string
}

func (prr UniverseEarnCoinTransactionByUserRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.CreatedAt,
		&prr.TransactionID,
		&prr.TransactionGroupID,
		&prr.Amount,
		&prr.Info,
	}
}

// ----------------------------------------------
// Table : universe.purchase_coin_transaction_by_user
// ----------------------------------------------

// CREATE TABLE universe.purchase_coin_transaction_by_user (
//    user_id UUID,
//    created_at TIMESTAMP,
//    transaction_id UUID,
//    transaction_group_id UUID,
//    amount BIGINT,
//    info TEXT,
//    PRIMARY KEY ((user_id), created_at, transaction_id)
// ) WITH CLUSTERING ORDER BY (created_at DESC);

const (
	TABLE_UNIVERSE_PurchaseCoinTransactionByUser                      = "purchase_coin_transaction_by_user"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_USER_ID              = "user_id"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_CREATED_AT           = "created_at"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_AMOUNT               = "amount"
	FIELD_UNIVERSE_PurchaseCoinTransactionByUser_INFO                 = "info"
)

// metadata specifies table name and columns it must be in sync with schema.
var universePurchaseCoinTransactionByUserTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_PurchaseCoinTransactionByUser,
	Columns: []string{
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_USER_ID,
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_CREATED_AT,
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_TRANSACTION_ID,
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_AMOUNT,
		FIELD_UNIVERSE_PurchaseCoinTransactionByUser_INFO,
	},
	PartKey: []string{FIELD_UNIVERSE_PurchaseCoinTransactionByUser_USER_ID},
	SortKey: []string{FIELD_UNIVERSE_PurchaseCoinTransactionByUser_CREATED_AT, FIELD_UNIVERSE_PurchaseCoinTransactionByUser_TRANSACTION_ID},
}

var UniversePurchaseCoinTransactionByUserTable = table.New(universePurchaseCoinTransactionByUserTableMetadata)

type UniversePurchaseCoinTransactionByUserRecord struct {
	UserID             string
	CreatedAt          int64
	TransactionID      string
	TransactionGroupID string
	Amount             int64
	Info               string
}

func (prr UniversePurchaseCoinTransactionByUserRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.UserID,
		&prr.CreatedAt,
		&prr.TransactionID,
		&prr.TransactionGroupID,
		&prr.Amount,
		&prr.Info,
	}
}

// ----------------------------------------------
// Table : universe.treasury_transaction_grant_coin
// ----------------------------------------------

// CREATE TABLE universe.treasury_transaction_grant_coin (
//    transaction_group_id UUID,
//    transaction_id UUID,
//    amount BIGINT,
//    PRIMARY KEY ((transaction_group_id), transaction_id)
// );

const (
	TABLE_UNIVERSE_TreasuryTransactionGrantCoin                      = "treasury_transaction_grant_coin"
	FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_TreasuryTransactionGrantCoin_AMOUNT               = "amount"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeTreasuryTransactionGrantCoinTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_TreasuryTransactionGrantCoin,
	Columns: []string{
		FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_ID,
		FIELD_UNIVERSE_TreasuryTransactionGrantCoin_AMOUNT,
	},
	PartKey: []string{FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_GROUP_ID},
	SortKey: []string{FIELD_UNIVERSE_TreasuryTransactionGrantCoin_TRANSACTION_ID},
}

var UniverseTreasuryTransactionGrantCoinTable = table.New(universeTreasuryTransactionGrantCoinTableMetadata)

type UniverseTreasuryTransactionGrantCoinRecord struct {
	TransactionGroupID string
	TransactionID      string
	Amount             int64
}

func (prr UniverseTreasuryTransactionGrantCoinRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.TransactionGroupID,
		&prr.TransactionID,
		&prr.Amount,
	}
}

// ----------------------------------------------
// Table : universe.treasury_transaction_dummy_coin
// ----------------------------------------------

// CREATE TABLE universe.treasury_transaction_dummy_coin (
//    transaction_group_id UUID,
//    transaction_id UUID,
//    amount BIGINT,
//    PRIMARY KEY ((transaction_group_id), transaction_id)
// );

const (
	TABLE_UNIVERSE_TreasuryTransactionDummyCoin                      = "treasury_transaction_dummy_coin"
	FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_TreasuryTransactionDummyCoin_AMOUNT               = "amount"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeTreasuryTransactionDummyCoinTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_TreasuryTransactionDummyCoin,
	Columns: []string{
		FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_ID,
		FIELD_UNIVERSE_TreasuryTransactionDummyCoin_AMOUNT,
	},
	PartKey: []string{FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_GROUP_ID},
	SortKey: []string{FIELD_UNIVERSE_TreasuryTransactionDummyCoin_TRANSACTION_ID},
}

var UniverseTreasuryTransactionDummyCoinTable = table.New(universeTreasuryTransactionDummyCoinTableMetadata)

type UniverseTreasuryTransactionDummyCoinRecord struct {
	TransactionGroupID string
	TransactionID      string
	Amount             int64
}

func (prr UniverseTreasuryTransactionDummyCoinRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.TransactionGroupID,
		&prr.TransactionID,
		&prr.Amount,
	}
}

// ----------------------------------------------
// Table : universe.treasury_transaction_earn_coin
// ----------------------------------------------

// CREATE TABLE universe.treasury_transaction_earn_coin (
//    transaction_group_id UUID,
//    transaction_id UUID,
//    amount BIGINT,
//    PRIMARY KEY ((transaction_group_id), transaction_id)
// );

const (
	TABLE_UNIVERSE_TreasuryTransactionEarnCoin                      = "treasury_transaction_earn_coin"
	FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_TreasuryTransactionEarnCoin_AMOUNT               = "amount"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeTreasuryTransactionEarnCoinTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_TreasuryTransactionEarnCoin,
	Columns: []string{
		FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_ID,
		FIELD_UNIVERSE_TreasuryTransactionEarnCoin_AMOUNT,
	},
	PartKey: []string{FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_GROUP_ID},
	SortKey: []string{FIELD_UNIVERSE_TreasuryTransactionEarnCoin_TRANSACTION_ID},
}

var UniverseTreasuryTransactionEarnCoinTable = table.New(universeTreasuryTransactionEarnCoinTableMetadata)

type UniverseTreasuryTransactionEarnCoinRecord struct {
	TransactionGroupID string
	TransactionID      string
	Amount             int64
}

func (prr UniverseTreasuryTransactionEarnCoinRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.TransactionGroupID,
		&prr.TransactionID,
		&prr.Amount,
	}
}

// ----------------------------------------------
// Table : universe.treasury_transaction_purchase_coin
// ----------------------------------------------

// CREATE TABLE universe.treasury_transaction_purchase_coin (
//    transaction_group_id UUID,
//    transaction_id UUID,
//    amount BIGINT,
//    PRIMARY KEY ((transaction_group_id), transaction_id)
// );

const (
	TABLE_UNIVERSE_TreasuryTransactionPurchaseCoin                      = "treasury_transaction_purchase_coin"
	FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_GROUP_ID = "transaction_group_id"
	FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_ID       = "transaction_id"
	FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_AMOUNT               = "amount"
)

// metadata specifies table name and columns it must be in sync with schema.
var universeTreasuryTransactionPurchaseCoinTableMetadata = table.Metadata{
	Name: TABLE_UNIVERSE_TreasuryTransactionPurchaseCoin,
	Columns: []string{
		FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_GROUP_ID,
		FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_ID,
		FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_AMOUNT,
	},
	PartKey: []string{FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_GROUP_ID},
	SortKey: []string{FIELD_UNIVERSE_TreasuryTransactionPurchaseCoin_TRANSACTION_ID},
}

var UniverseTreasuryTransactionPurchaseCoinTable = table.New(universeTreasuryTransactionPurchaseCoinTableMetadata)

type UniverseTreasuryTransactionPurchaseCoinRecord struct {
	TransactionGroupID string
	TransactionID      string
	Amount             int64
}

func (prr UniverseTreasuryTransactionPurchaseCoinRecord) Unpack() []interface{} {
	return []interface{}{
		&prr.TransactionGroupID,
		&prr.TransactionID,
		&prr.Amount,
	}
}
