package main

import (
	"time"

	"github.com/bwmarrin/discordgo"
)

var (
	day1NotificationTickerDuration = 60 * time.Minute
)

func registerDay1NotificationTicker(dgs *discordgo.Session) {

	opts := Day1NotificationsOpts{
		Dryrun:      false,
		MinimalLogs: true,
	}

	// send day 1 notifications
	sendDay1Notification(dgs, opts)

	ticker := time.NewTicker(day1NotificationTickerDuration)
	done := make(chan bool)
	go func() {
		for {
			select {
			case <-done:
				return
			case _ = <-ticker.C:
				// fmt.Println("Tick at", _)
				// dg.ChannelMessageSend(CHANNEL_BOT_TESTS_ID, "Day 1 notification Ticker")

				// send day 1 notifications
				sendDay1Notification(dgs, opts)
			}
		}
	}()
}
