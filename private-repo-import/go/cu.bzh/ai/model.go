package ai

// Model is an interface for an AI model.
type Model interface {

	// Name returns the name of the model.
	Name() string

	// Generate generates a response from the model.
	GenerateSimple(userPrompt string, opts GenerateOpts) (string, error)
	// GenerateSimpleStream(userPrompt string, opts GenerateOpts) (chan string, error)

	Generate(messages []Message, opts GenerateOpts) (string, error)
	GenerateStream(messages []Message, opts GenerateOpts) (chan string, error)
}
