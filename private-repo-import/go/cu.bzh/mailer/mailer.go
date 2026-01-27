package mailer

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"regexp"
	"strings"

	"github.com/vanng822/go-premailer/premailer"
	"golang.org/x/text/encoding/charmap"

	htmlTemplate "html/template"
	txtTemplate "text/template"

	"github.com/gdevillele/frontparser"
	blackfriday "github.com/russross/blackfriday/v2"

	mailjet "github.com/mailjet/mailjet-apiv3-go"
	mailjetResources "github.com/mailjet/mailjet-apiv3-go/resources"
)

var (
	templateTXT  *txtTemplate.Template
	templateHTML *htmlTemplate.Template
)

// Send ...
func Send(toAddr, toName, templatePath, markdownPath, mailjetAPIKey, mailjetAPISecret string, content interface{}, customID string) error {
	// LOAD TEMPLATES

	htmlTemplateBytes, err := os.ReadFile(templatePath)
	if err != nil {
		return err
	}

	temporaryTemplateHTML, err := txtTemplate.New("template-html").Parse(string(htmlTemplateBytes))
	if err != nil {
		return err
	}

	// PARSE MARKDOWN

	parsedMarkdown, err := parseMarkdown(markdownPath, content)
	if err != nil {
		return err
	}

	// USE MARKDOWN AS HTML CONTENT

	buf := &bytes.Buffer{}
	err = temporaryTemplateHTML.Execute(buf, parsedMarkdown)
	if err != nil {
		return err
	}

	// BUILD FINAL HTML TEMPLATE

	templateHTML, err = htmlTemplate.New("template-html").Parse(buf.String())
	if err != nil {
		return err
	}

	templateTXT, err = txtTemplate.New("template-txt").Parse(parsedMarkdown.Txt)
	if err != nil {
		return err
	}

	// SEND

	if toAddr == "" {
		return errors.New("recipient email address can't be empty")
	}

	buf = &bytes.Buffer{}
	err = templateHTML.Execute(buf, content)
	if err != nil {
		return err
	}
	htmlContent := buf.String()

	// INLINE CSS

	prem, err := premailer.NewPremailerFromString(htmlContent, premailer.NewOptions())
	if err != nil {
		fmt.Println("❌ CSS INLINER ERROR (1):", err)
		return err
	}

	htmlContent, err = prem.Transform()
	if err != nil {
		fmt.Println("❌ CSS INLINER ERROR (2):", err)
		return err
	}

	// fmt.Println("buf:", htmlContent)

	buf = &bytes.Buffer{}
	err = templateTXT.Execute(buf, content)
	if err != nil {
		return err
	}
	plainTextContent := buf.String()

	messagesInfo := []mailjet.InfoMessagesV31{
		{
			From: &mailjet.RecipientV31{
				Email: parsedMarkdown.SenderEmail,
				Name:  parsedMarkdown.SenderName,
			},
			To: &mailjet.RecipientsV31{
				mailjet.RecipientV31{
					Email: toAddr,
					Name:  toName,
				},
			},
			Subject:  parsedMarkdown.Title,
			TextPart: plainTextContent,
			HTMLPart: htmlContent,
			CustomID: customID,
		},
	}
	messages := mailjet.MessagesV31{Info: messagesInfo}

	// Create mailjet client
	mailjetClient := mailjet.NewMailjetClient(mailjetAPIKey, mailjetAPISecret)
	if mailjetClient == nil {
		return errors.New("failed to create mailjet client")
	}

	// Send email
	_, err = mailjetClient.SendMailV31(&messages)
	if err != nil {
		return err
	}

	fmt.Println("email sent to", toAddr)
	return nil
}

// EmailAndName stores email/name pairs
type Contact struct {
	Email      string
	Name       string
	Properties *ContactProperties
}

type ContactProperties struct {
	UserID        string `json:"userid,omitempty"`
	Username      string `json:"username,omitempty"`
	Usernumber    int    `json:"usernumber,omitempty"`
	Greetings     string `json:"greetings,omitempty"`
	SteamKey      string `json:"steamkey,omitempty"`
	HasBetaAccess bool   `json:"access,omitempty"`
	Email         string `json:"useremail,omitempty"`
}

func AddContacts(mailjetAPIKey string, mailjetAPISecret string, contacts []*Contact, listID int64) error {

	fmt.Println("syncing", len(contacts), "contacts")

	addContactActions := make([]mailjetResources.AddContactAction, len(contacts))

	for i, c := range contacts {
		addContactActions[i] = mailjetResources.AddContactAction{
			Email:      c.Email,
			Name:       c.Name,
			Properties: c.Properties,
		}
	}

	mailjetClient := mailjet.NewMailjetClient(mailjetAPIKey, mailjetAPISecret)

	var data []mailjetResources.ContactslistManageManyContacts
	mr := &mailjet.Request{
		Resource: "contactslist",
		ID:       listID, // Particubes players list
		Action:   "managemanycontacts",
	}

	fmr := &mailjet.FullRequest{
		Info: mr,
		Payload: &mailjetResources.ContactslistManageManyContacts{
			Action:   "addnoforce",
			Contacts: addContactActions,
		},
	}

	err := mailjetClient.Post(fmr, &data)
	if err != nil {
		return err
	}
	// fmt.Printf("Data array: %+v\n", data)
	return nil
}

// ParsedMarkdown ...
type ParsedMarkdown struct {
	HTML        string
	Txt         string
	Title       string
	Description string
	Keywords    string
	SenderEmail string
	SenderName  string
}

func parseMarkdown(markdownPath string, content interface{}) (*ParsedMarkdown, error) {

	// read markdown file
	mdFileBytes, err := os.ReadFile(markdownPath)
	if err != nil {
		return nil, err
	}

	// inject content in template
	mdTemplate, err := txtTemplate.New("template-feedback-markdown").Parse(string(mdFileBytes))
	if err != nil {
		return nil, err
	}
	buf := &bytes.Buffer{}
	err = mdTemplate.Execute(buf, content)
	if err != nil {
		return nil, err
	}
	mdFileBytes = buf.Bytes()

	resp := &ParsedMarkdown{}

	// page's default info
	pageMdContent := mdFileBytes
	pageFrontmatter := make(map[string]interface{})

	// frontmatter parsing
	// check if file has a frontmatter header
	if frontparser.HasFrontmatterHeader(mdFileBytes) {
		fm, md, err := frontparser.ParseFrontmatterAndContent(mdFileBytes)
		if err != nil {
			return nil, err
		}
		pageFrontmatter = fm
		pageMdContent = md

		// find title in frontmatter
		if titleIface, ok := pageFrontmatter["title"]; ok {
			titleStr, err := toString(titleIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter title value is not a string.")
			} else {
				resp.Title = titleStr
			}
		}

		// find keywords in frontmatter
		if keywordsIface, ok := pageFrontmatter["keywords"]; ok {
			keywordsStr, err := toString(keywordsIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter keywords value is not a string")
			} else {
				resp.Keywords = keywordsStr
			}
		}
		// find description in frontmatter
		if descriptionIface, ok := pageFrontmatter["description"]; ok {
			descriptionStr, err := toString(descriptionIface)
			if err != nil {
				fmt.Println("ERROR: frontmatter description value is not a string")
			} else {
				resp.Description = descriptionStr
			}
		}

		// find sender-email in frontmatter
		if templateIface, ok := pageFrontmatter["sender-email"]; ok {
			templateStr, err := toString(templateIface)
			if err != nil {
				fmt.Println("ERROR: sender-email template value is not a string")
			} else {
				resp.SenderEmail = templateStr
			}
		}

		// find sender-name in frontmatter
		if templateIface, ok := pageFrontmatter["sender-name"]; ok {
			templateStr, err := toString(templateIface)
			if err != nil {
				fmt.Println("ERROR: sender-name template value is not a string")
			} else {
				resp.SenderName = templateStr
			}
		}
	}

	resp.Txt = string(pageMdContent)
	resp.HTML = string(blackfriday.Run(pageMdContent))

	return resp, nil
}

////////////////////////////////////////////////////////////
///
/// Utility functions
///
////////////////////////////////////////////////////////////

func toString(i interface{}) (string, error) {
	if str, ok := i.(string); ok {
		return str, nil
	}
	return "", errors.New("interface is not a string")
}

// opens file and returns reader with adapted decoder
func fileReader(filePath string) (io.Reader, *os.File, error) {

	file, err := os.Open(filePath)
	if err != nil {
		return nil, nil, err
	}

	// NOTE: only works on MacOS
	cmd := exec.Command("file", "-I", filePath)
	b, err := cmd.Output()
	if err != nil {
		file.Close()
		return nil, nil, err
	}

	cmdOutput := string(b)

	re := regexp.MustCompile("charset=([-a-zA-Z0-9]+)")

	matches := re.FindStringSubmatch(cmdOutput)

	if len(matches) < 2 {
		// can't find charset, use default reader
		return file, file, nil
	}

	fileCharset := matches[1]

	fileCharset = strings.Replace(fileCharset, "-", "_", -1)
	fileCharset = strings.Replace(fileCharset, " ", "_", -1)
	fileCharset = strings.ToUpper(fileCharset)

	for _, encoding := range charmap.All {

		knownCharset := fmt.Sprint(encoding)

		knownCharset = strings.Replace(knownCharset, "-", "_", -1)
		knownCharset = strings.Replace(knownCharset, " ", "_", -1)
		knownCharset = strings.ToUpper(knownCharset)

		if fileCharset == knownCharset {
			r := encoding.NewDecoder().Reader(file)
			return r, file, nil
		}
	}

	// can't find charset, use default reader
	return file, file, nil
}
