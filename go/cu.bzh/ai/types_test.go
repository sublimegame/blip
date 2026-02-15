package ai

import (
	"encoding/json"
	"testing"
)

// ------------------------------------
// SystemPrompt
// ------------------------------------

func TestSystemPrompt1(t *testing.T) {
	systemPrompt := NewSystemPrompt()
	systemPrompt.Append(NewTextPart("part1"))
	systemPrompt.Append(NewTextPart("part2"))
	systemPrompt.Append(NewTextPart("part3"))

	// String()
	if systemPrompt.String() != "part1\npart2\npart3" {
		t.Errorf("SystemPrompt.String() = %s; want %s", systemPrompt.String(), "part1\npart2\npart3")
	}

	// Array()
	if len(systemPrompt.Array()) != 3 {
		t.Errorf("SystemPrompt.Array() = %d; want %d", len(systemPrompt.Array()), 3)
	}
	if systemPrompt.Array()[0] != "part1" {
		t.Errorf("SystemPrompt.Array()[0] = %s; want %s", systemPrompt.Array()[0], "part1")
	}
	if systemPrompt.Array()[1] != "part2" {
		t.Errorf("SystemPrompt.Array()[1] = %s; want %s", systemPrompt.Array()[1], "part2")
	}
	if systemPrompt.Array()[2] != "part3" {
		t.Errorf("SystemPrompt.Array()[2] = %s; want %s", systemPrompt.Array()[2], "part3")
	}
}

func TestSystemPrompt2(t *testing.T) {
	const content = "this is the 1st part"

	systemPrompt := NewSystemPromptFromText(content)

	// String()
	if systemPrompt.String() != content {
		t.Errorf("SystemPrompt.String() = %s; want %s", systemPrompt.String(), content)
	}

	// Array()
	array := systemPrompt.Array()
	if len(array) != 1 {
		t.Errorf("SystemPrompt.Array() = %d; want %d", len(array), 1)
	}
	if array[0] != content {
		t.Errorf("SystemPrompt.Array()[0] = %s; want %s", array[0], content)
	}
}

func TestSystemPrompt3(t *testing.T) {
	const content = "this is the 1st part"

	systemPrompt := NewSystemPromptFromText(content)
	systemPrompt.Append(NewTextPart("this is the 2nd part"))

	// encode into json
	json, err := json.Marshal(systemPrompt)
	if err != nil {
		t.Errorf("SystemPrompt.MarshalJSON() error: %v", err)
	}

	expectedJSONStr := `[{"text":"this is the 1st part"},{"text":"this is the 2nd part"}]`

	if string(json) != expectedJSONStr {
		t.Errorf("SystemPrompt.MarshalJSON() = %s; want %s", string(json), expectedJSONStr)
	}
}
