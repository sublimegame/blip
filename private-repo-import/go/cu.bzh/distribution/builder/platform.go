package main

import (
	"runtime"
)

const (
	platformOSLinux   = "linux"
	platformOSWindows = "windows"
	platformOSDarwin  = "darwin"

	platformArchAmd64 = "amd64"
	platformArchArm64 = "arm64"
)

// OS

func platformOS() string {
	return runtime.GOOS
}

func platformOSIsLinux() bool {
	return platformOS() == platformOSLinux
}

func platformOSIsWindows() bool {
	return platformOS() == platformOSWindows
}

func platformOSIsDarwin() bool {
	return platformOS() == platformOSDarwin
}

// CPU Arch

func platformArch() string {
	return runtime.GOARCH
}

func platformArchIsAmd64() bool {
	return platformArch() == platformArchAmd64
}

func platformArchIsArm64() bool {
	return platformArch() == platformArchArm64
}
