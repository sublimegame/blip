package main

type VisualStudioTool struct{}

func (t VisualStudioTool) Name() string {
	return "VisualStudio"
}

func (t VisualStudioTool) IsInstalled() bool {
	return false
}
