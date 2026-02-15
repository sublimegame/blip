package main

import (
	"errors"
	"fmt"
)

// Wasm target for app.cu.bzh

type WasmTarget struct{}

func (t WasmTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t WasmTarget) Name() string {
	return targetWasm
}

func (t WasmTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin, platformOSLinux, platformOSWindows}
}

func (t WasmTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t WasmTarget) CompatibleToolset() []Tool {
	return []Tool{toolDocker}
}

func (t WasmTarget) RunPrebuildChecks() error {
	return nil
}

func (t WasmTarget) Build(opts BuildOpts) error {
	return errors.New("not implemented")
}

// Wasm target for Discord Activity

type WasmDiscordTarget struct{}

func (t WasmDiscordTarget) ToString() string {
	return fmt.Sprintf("🎯 %s", t.Name())
}

func (t WasmDiscordTarget) Name() string {
	return targetWasmDiscord
}

func (t WasmDiscordTarget) CompatibleOSes() []string {
	return []string{platformOSDarwin, platformOSLinux, platformOSWindows}
}

func (t WasmDiscordTarget) CompatibleArchs() []string {
	return []string{platformArchAmd64, platformArchArm64}
}

func (t WasmDiscordTarget) CompatibleToolset() []Tool {
	return []Tool{toolDocker}
}

func (t WasmDiscordTarget) RunPrebuildChecks() error {
	return nil
}

func (t WasmDiscordTarget) Build(opts BuildOpts) error {
	return errors.New("not implemented")
}
