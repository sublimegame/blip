package anthropic

import (
	"testing"

	"cu.bzh/ai"
)

// TestGeminiModelImplementsModelInterface ensures that GeminiModel implements the ai.Model interface
func TestAnthropicModelImplementsModelInterface(t *testing.T) {
	// This is a compile-time check to ensure AnthropicModel implements ai.Model
	var _ ai.Model = (*AnthropicModel)(nil)
}
