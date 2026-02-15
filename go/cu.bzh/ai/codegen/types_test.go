package main

import (
	"testing"

	"cu.bzh/ai"
)

func TestSystemPromptString(t *testing.T) {
	tests := []struct {
		name     string
		parts    ai.SystemPrompt
		expected string
	}{
		{
			name:     "empty prompt",
			parts:    ai.SystemPrompt{},
			expected: "",
		},
		{
			name: "single text part",
			parts: ai.SystemPrompt{
				ai.NewTextPart("Hello, world!"),
			},
			expected: "Hello, world!",
		},
		{
			name: "multiple text parts",
			parts: ai.SystemPrompt{
				ai.NewTextPart("First line"),
				ai.NewTextPart("Second line"),
				ai.NewTextPart("Third line"),
			},
			expected: "First line\nSecond line\nThird line",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := tt.parts.String()
			if got != tt.expected {
				t.Errorf("String() = %q, want %q", got, tt.expected)
			}
		})
	}
}
