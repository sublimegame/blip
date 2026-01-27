package transactions

import (
	"errors"

	"cu.bzh/hub/scyllaclient"
)

type GrantCoinsOpts struct {
	// If true, the grant will fail if the user already received a grant having the same reason.
	// False by default.
	Unique bool
	// Description of the grant will be displayed in the user's transaction history.
	Description string
}

func NewGrantCoinsOpts(unique bool, description string) GrantCoinsOpts {
	return GrantCoinsOpts{
		Unique:      unique,
		Description: description,
	}
}

func GrantCoinsToUser(userID string, amountMillionth int64, reason string, opts GrantCoinsOpts) error {

	if amountMillionth == 0 {
		return errors.New("amount is 0")
	}

	if reason == "" {
		return errors.New("reason is empty")
	}

	scyllaClientUniverse := scyllaclient.GetShared("universe")
	if scyllaClientUniverse == nil {
		return errors.New("failed to get scyllaDB client")
	}

	// If grant is unique, we check if the user already received a grant having the same reason
	if opts.Unique {
		// check if the user already received a grant having the same reason
		found, err := scyllaClientUniverse.UserHasCoinGrantForReason(userID, reason)
		if err != nil {
			return err
		}
		if found {
			return errors.New("user already received a grant having the same reason")
		}
	}

	// grant coins
	err := scyllaClientUniverse.GrantUserCoin(userID, amountMillionth, reason, opts.Description)
	if err != nil {
		return err
	}

	return nil
}

type CreditPurchasedCoinsOpts struct {
	Description string
}

func NewCreditPurchasedCoinsOpts(description string) CreditPurchasedCoinsOpts {
	return CreditPurchasedCoinsOpts{Description: description}
}

func CreditPurchasedCoinsToUser(userID string, amountMillionth int64, reason string, opts CreditPurchasedCoinsOpts) error {
	if amountMillionth == 0 {
		return errors.New("amount is 0")
	}

	if reason == "" {
		return errors.New("reason is empty")
	}

	scyllaClientUniverse := scyllaclient.GetShared("universe")
	if scyllaClientUniverse == nil {
		return errors.New("failed to get scyllaDB client")
	}

	// check if the user already received a grant having the same reason
	found, err := scyllaClientUniverse.UserReceivedPurchasedCoinsForReason(userID, reason)
	if err != nil {
		return err
	}
	if found {
		return errors.New("user already received purchased coins")
	}

	// grant coins
	err = scyllaClientUniverse.CreditUserPurchasedCoins(userID, amountMillionth, reason, opts.Description)
	if err != nil {
		return err
	}

	return nil
}

type DebitCoinsFromUserOpts struct {
	// at least one of the amounts must be > 0
	GrantedCoinAmountMillionth   int64
	PurchasedCoinAmountMillionth int64
	EarnedCoinAmountMillionth    int64
	Reason                       string // mandatory
	Description                  string // optional
}

// DebitCoinsFromUser debits coins from a user.
// The amount is in millionths of coins.
// The reason is a short string describing the reason for the debit (e.g. "payment for <foo>"). It is mandatory.
// The description is a longer string that can be displayed to users. It is optional.
func DebitCoinsFromUser(userID string, opts DebitCoinsFromUserOpts) error {

	// not a single amount can be < 0
	if opts.GrantedCoinAmountMillionth < 0 ||
		opts.PurchasedCoinAmountMillionth < 0 ||
		opts.EarnedCoinAmountMillionth < 0 {
		return errors.New("amount is negative")
	}

	// at least one amount must be > 0
	if opts.GrantedCoinAmountMillionth == 0 &&
		opts.PurchasedCoinAmountMillionth == 0 &&
		opts.EarnedCoinAmountMillionth == 0 {
		return errors.New("amount is 0")
	}

	if opts.Reason == "" {
		return errors.New("reason is empty")
	}

	scyllaClientUniverse := scyllaclient.GetShared("universe")
	if scyllaClientUniverse == nil {
		return errors.New("failed to get scyllaDB client")
	}

	// debit coins
	err := scyllaClientUniverse.UserMakesPaymentForReason(
		userID,
		opts.GrantedCoinAmountMillionth,
		opts.PurchasedCoinAmountMillionth,
		opts.EarnedCoinAmountMillionth,
		opts.Reason,
		opts.Description,
	)
	if err != nil {
		return err
	}

	return nil
}
