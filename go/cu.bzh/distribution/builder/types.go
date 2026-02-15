package main

import "fmt"

type Target interface {
	ToString() string
	Name() string
	CompatibleOSes() []string
	CompatibleArchs() []string
	CompatibleToolset() []Tool
	RunPrebuildChecks() error
	Build(opts BuildOpts) error
}

type ErrIncompatibleOS struct {
	currentOS      string
	compatibleOSes []string
}

func (e *ErrIncompatibleOS) Error() string {
	msg := fmt.Sprintf("OS is not compatible. Expected one of %v, got %s", e.compatibleOSes, e.currentOS)
	return msg
}

type ErrIncompatibleArch struct {
	currentArch     string
	compatibleArchs []string
}

func (e *ErrIncompatibleArch) Error() string {
	msg := fmt.Sprintf("OS is not compatible. Expected one of %v, got %s", e.compatibleArchs, e.currentArch)
	return msg
}

type ErrMissingTooling struct {
	ToolName string
}

func (e *ErrMissingTooling) Error() string {
	return "tool is missing: " + e.ToolName
}

// Used to indicate whether a target is supported on the machine
type targetCompatbilityStatus struct {
	TargetName  string
	IsSupported bool
	Error       error
}

func (t targetCompatbilityStatus) ToString() string {
	if t.IsSupported {
		return fmt.Sprintf("✅ %s", t.TargetName)
	}
	return fmt.Sprintf("❌ %s", t.TargetName)
}

// Tool

type Tool interface {
	Name() string
	IsInstalled() bool
}

//

type BuildOpts struct {
	ReleaseMode  bool
	ForceRebuild bool // Forces a clean before re-building the entire project.
}

type BuildMacros map[string]string
