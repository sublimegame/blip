package main

import "os/exec"

type DockerTool struct{}

func (t DockerTool) Name() string {
	return "Docker"
}

func (t DockerTool) IsInstalled() bool {
	// Execute "docker version" command
	cmd := exec.Command("docker", "version")

	// Run the command and check for errors
	if err := cmd.Run(); err != nil {
		return false
	}

	return true
}
