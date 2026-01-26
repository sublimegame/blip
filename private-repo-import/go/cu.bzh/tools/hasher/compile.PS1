# Set the script to exit immediately if a command exits with a non-zero status
$ErrorActionPreference = "Stop"

# Save the current location
$START_LOCATION = Get-Location

# Get the location of the script
$SCRIPT_LOCATION = (Get-Item -Path $MyInvocation.MyCommand.Definition).Directory.FullName

# Define a function to revert to the original location when the script exits
function OnExit {
    Set-Location $START_LOCATION
}
# Register the function to run on script exit
$null = Register-EngineEvent PowerShell.Exiting -Action { OnExit }

# Go to the script location before running git command
# to make sure it runs within project tree
Set-Location $SCRIPT_LOCATION

# Use git command to get root project directory
$PROJECT_ROOT = & git rev-parse --show-toplevel

# The script is now executed from project root directory
Set-Location $SCRIPT_LOCATION # Set-Location $PROJECT_ROOT

# Destination directory for compiled binaries
$binariesPath = "$PROJECT_ROOT/deps/hasher"

# Compile for Linux (Intel 64-bit)
$env:GOOS = "linux"
$env:GOARCH = "amd64"
go build -o "$binariesPath/linux/amd64/hasher"

# Compile for Linux (ARM 64-bit)
$env:GOOS = "linux"
$env:GOARCH = "arm64"
go build -o "$binariesPath/linux/arm64/hasher"

# Compile for macOS (Intel 64-bit)
$env:GOOS = "darwin"
$env:GOARCH = "amd64"
go build -o "$binariesPath/macos/amd64/hasher"

# Compile for macOS (Apple Silicon)
$env:GOOS = "darwin"
$env:GOARCH = "arm64"
go build -o "$binariesPath/macos/arm64/hasher"

# Compile for Windows (64-bit)
$env:GOOS = "windows"
$env:GOARCH = "amd64"
go build -o "$binariesPath/windows/amd64/hasher.exe"
