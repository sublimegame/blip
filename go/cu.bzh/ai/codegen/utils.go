package main

import (
	"fmt"

	"cu.bzh/ai"
	"cu.bzh/ai/anthropic"
	"cu.bzh/ai/gemini"
	"cu.bzh/ai/grok"
)

func getApiKeyForModelName(modelName string) (string, error) {
	if anthropic.IsModelSupported(modelName) {
		return anthropic_api_key, nil
	} else if gemini.IsModelSupported(modelName) {
		return gemini_api_key, nil
	} else if grok.IsModelSupported(modelName) {
		return grok_api_key, nil
	} else if modelName == MODEL_NAME_LOCAL {
		return "", nil
	} else if modelName == "" { // LEGACY (needs to be removed later)
		return anthropic_api_key, nil
	}
	return "", fmt.Errorf("unsupported model: %s", modelName)
}

func getModelForModelName(modelName string) (ai.Model, error) {
	var modelToUse ai.Model
	if anthropic.IsModelSupported(modelName) {
		modelToUse = anthropic.NewModel(modelName)
	} else if gemini.IsModelSupported(modelName) {
		modelToUse = gemini.NewModel(modelName)
	} else if grok.IsModelSupported(modelName) {
		modelToUse = grok.NewModel(modelName)
	} else {
		return nil, fmt.Errorf("unsupported model: %s", modelName)
	}
	return modelToUse, nil
}
