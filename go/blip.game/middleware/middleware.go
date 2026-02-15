package middleware

import (
	"context"
	"net/http"
)

const (
	// Request context fields
	kRequestContextFieldUserCredentials = "vx_user_credentials"

	// Constants
	kHTTPHeaderUserID          = "Czh-Usr-Id"
	kHTTPHeaderUserToken       = "Czh-Tk"
	kHTTPHeaderGameServerToken = "Czh-Server-Token"
)

// --------------------------------------------------
// Types
// --------------------------------------------------

type UserCredentials struct {
	UserID      string
	Token       string
	ServerToken string
}

func (c *UserCredentials) IsEmpty() bool {
	return c.UserID == "" && c.Token == "" && c.ServerToken == ""
}

// --------------------------------------------------
// Middlewares
// --------------------------------------------------

// ReadUserCredentialsMiddleware reads user credentials from the request headers
// and adds them to the request context.
func ReadUserCredentialsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// get header values

		credentials := UserCredentials{
			UserID:      r.Header.Get(kHTTPHeaderUserID),
			Token:       r.Header.Get(kHTTPHeaderUserToken),
			ServerToken: r.Header.Get(kHTTPHeaderGameServerToken),
		}

		if credentials.IsEmpty() {
			// There is no user credentials to add to the context.
			// Continue the request processing.
			next.ServeHTTP(w, r)
			return
		}

		// add user credentials to the context

		ctx := context.WithValue(r.Context(), kRequestContextFieldUserCredentials, credentials)

		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// --------------------------------------------------
// Context fields accessors
// --------------------------------------------------

// GetUserCredentialsFromContext returns the user credentials from the request context.
func GetUserCredentialsFromRequestContext(requestContext context.Context) *UserCredentials {

	rawCredentials := requestContext.Value(kRequestContextFieldUserCredentials)
	if rawCredentials == nil {
		return nil
	}

	credentials, ok := rawCredentials.(UserCredentials)
	if !ok {
		return nil
	}

	return &credentials
}
