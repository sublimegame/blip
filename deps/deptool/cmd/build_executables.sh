#!/bin/bash

# Build executables for each platform

# Build for macos
GOOS=darwin GOARCH=arm64 go build -o deptool_macos_arm64
echo "✅ deptool_macos_arm64 built"
GOOS=darwin GOARCH=amd64 go build -o deptool_macos_x86_64
echo "✅ deptool_macos_x86_64 built"

# Build for windows
GOOS=windows GOARCH=amd64 go build -o deptool_windows_x86_64.exe
echo "✅ deptool_windows_x86_64.exe built"
# GOOS=windows GOARCH=arm64 go build -o deptool_windows_arm64.exe

# Build for linux
GOOS=linux GOARCH=arm64 go build -o deptool_linux_arm64
echo "✅ deptool_linux_arm64 built"
GOOS=linux GOARCH=amd64 go build -o deptool_linux_x86_64
echo "✅ deptool_linux_x86_64 built"
