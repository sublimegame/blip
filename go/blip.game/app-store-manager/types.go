package main

type BlockType string

const (
	BlockTypeApp          BlockType = "apps"
	BlockTypeAppInfo      BlockType = "appInfos"
	BlockTypeLocalization BlockType = "appInfoLocalizations"
)

type BlockAttributes struct {
	Name             string `json:"name,omitempty"`
	Locale           string `json:"locale,omitempty"`
	Subtitle         string `json:"subtitle,omitempty"`
	PrivacyPolicyURL string `json:"privacyPolicyUrl,omitempty"`
}

type Block struct {
	ID         string            `json:"id,omitempty"`
	Type       string            `json:"type,omitempty"`
	Attributes *BlockAttributes  `json:"attributes,omitempty"`
	Links      map[string]string `json:"links,omitempty"`
}

type Response struct {
	Data     []*Block `json:"data,omitempty"`
	Included []*Block `json:"included,omitempty"`
}

type App struct {
	ID   string `json:"id,omitempty"`
	Name string `json:"name,omitempty"`
}

type Localization struct {
	ID               string `json:"id,omitempty"`
	Name             string `json:"name,omitempty"`
	Locale           string `json:"locale,omitempty"`
	Subtitle         string `json:"subtitle,omitempty"`
	PrivacyPolicyURL string `json:"privacyPolicyUrl,omitempty"`
}

func LocalizationToBlock(l *Localization) *Block {
	return &Block{
		ID:   l.ID,
		Type: string(BlockTypeLocalization),
		Attributes: &BlockAttributes{
			Name:             l.Name,
			Locale:           l.Locale,
			Subtitle:         l.Subtitle,
			PrivacyPolicyURL: l.PrivacyPolicyURL,
		},
	}
}

func BlockToLocalization(b *Block) (*Localization, bool) {
	if b.Type != string(BlockTypeLocalization) {
		return nil, false
	}
	if b.Attributes == nil {
		return nil, false
	}

	return &Localization{
		ID:               b.ID,
		Name:             b.Attributes.Name,
		Locale:           b.Attributes.Locale,
		Subtitle:         b.Attributes.Subtitle,
		PrivacyPolicyURL: b.Attributes.PrivacyPolicyURL,
	}, true
}
