package main

import (
	"errors"
	"fmt"
)

// Windows target for Steam

type WindowsSteamTarget struct{}

func (t WindowsSteamTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t WindowsSteamTarget) Name() string {
	return targetWindowsSteam
}

func (t WindowsSteamTarget) CompatibleOSes() []string {
	return []string{platformOSWindows}
}

func (t WindowsSteamTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64}
}

func (t WindowsSteamTarget) CompatibleToolset() []Tool {
	return []Tool{toolDocker, toolVisualStudio}
}

func (t WindowsSteamTarget) RunPrebuildChecks() error {
	return nil
}

func (t WindowsSteamTarget) Build(opts BuildOpts) error {
	return errors.New("not implemented")
}

// Windows target for Epic Games Store

type WindowsEGSTarget struct{}

func (t WindowsEGSTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t WindowsEGSTarget) Name() string {
	return targetWindowsEpicGamesStore
}

func (t WindowsEGSTarget) CompatibleOSes() []string {
	return []string{platformOSWindows}
}

func (t WindowsEGSTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64}
}

func (t WindowsEGSTarget) CompatibleToolset() []Tool {
	return []Tool{toolDocker, toolVisualStudio}
}

func (t WindowsEGSTarget) RunPrebuildChecks() error {
	return nil
}

func (t WindowsEGSTarget) Build(opts BuildOpts) error {
	return errors.New("not implemented")
}
