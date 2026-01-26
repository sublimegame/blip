package discord

import (
	"errors"

	"github.com/bwmarrin/discordgo"
)

type Client struct {
	dg *discordgo.Session
}

func NewClient(token string) (*Client, error) {

	if token == "" {
		return nil, errors.New("token is missing")
	}

	dg, err := discordgo.New("Bot " + token)
	if err != nil {
		return nil, err
	}

	return &Client{
		dg: dg,
	}, nil
}

// SendMessage sends a message to a channel
// Returns the message ID or an error if the message fails to send
func (c *Client) SendMessage(channelID string, message string) error {
	_, err := c.dg.ChannelMessageSend(channelID, message)
	return err
}

// // EditMessage edits a message in a channel
// // Returns an error if the message fails to edit
// func (c *Client) EditMessage(channelID, messageID, message string) error {
// 	_, err := c.dg.ChannelMessageEdit(channelID, messageID, message)
// 	return err
// }

// // DeleteMessage deletes a message in a channel
// // Returns an error if the message fails to delete
// func (c *Client) DeleteMessage(channelID, messageID string) error {
// 	err := c.dg.ChannelMessageDelete(channelID, messageID)
// 	return err
// }
