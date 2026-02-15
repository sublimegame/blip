package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"slices"
	"strconv"

	"blip.game/middleware"
	"cu.bzh/hub/dbclient"
	"cu.bzh/hub/user"
	"cu.bzh/sessionstore"
	"cu.bzh/utils"
)

// --------------------------------------------------
//
// Types
//
// --------------------------------------------------

const (
	// Keys used to store info in request objects
	REQ_CLIENTVERSION            string = "czhClientVersion"
	REQ_CLIENTBUILDNUMBER        string = "czhClientBuildNumber"
	CONTEXT_FIELD_USER           string = "usr"
	CONTEXT_SESSION_KEY          string = "session"
	SESSION_KEY_PRIVILEGED_TOKEN string = "privilegedTk"
)

// --------------------------------------------------
//
// Middlewares
//
// --------------------------------------------------

// authenticateUserMiddleware gets user credentials from request context and validates them.
// If credentials are invalid, it returns a 401 Unauthorized error.
// If credentials are valid, it adds the user to the request context.
func authenticateUserMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// get user credentials from request context (can be nil)
		credentials := middleware.GetUserCredentialsFromRequestContext(r.Context())

		// if credentials are missing, return 401 Unauthorized
		if credentials == nil {
			respondWithError(http.StatusUnauthorized, "", w)
			return
		}

		// get database client
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		usr, err := user.GetWithIDAndToken(dbClient, credentials.UserID, credentials.Token)
		if err != nil {
			// failed to get user from database
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if usr == nil {
			// user not found in database
			respondWithError(http.StatusUnauthorized, "", w)
			return
		}

		// got user from database with valid credentials
		// add user to request context
		ctx := context.WithValue(r.Context(), CONTEXT_FIELD_USER, usr)

		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// tries to authenticate user, it's ok if it fails
func optionalAuthMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// get user credentials from request context (can be nil)
		credentials := middleware.GetUserCredentialsFromRequestContext(r.Context())

		// if credentials are missing, continue the request processing
		if credentials == nil {
			next.ServeHTTP(w, r)
			return
		}

		// get database client
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		usr, err := user.GetWithIDAndToken(dbClient, credentials.UserID, credentials.Token)
		if err != nil {
			// failed to get user from database
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if usr == nil {
			// user not found in database
			// continue the request processing
			next.ServeHTTP(w, r)
			return
		}

		// got user from database with valid credentials
		// add user to request context
		ctx := context.WithValue(r.Context(), CONTEXT_FIELD_USER, usr)
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// Lets users and gameservers go through
func userAndGameServerAuthMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// get user credentials from request context (can be nil)
		credentials := middleware.GetUserCredentialsFromRequestContext(r.Context())

		// if credentials are missing, return 401 Unauthorized
		if credentials == nil {
			respondWithError(http.StatusUnauthorized, "", w)
			return
		}

		// check if user is a gameserver
		if credentials.ServerToken == HTTP_HEADER_CUBZH_GAME_SERVER_TOKEN_VALUE {
			next.ServeHTTP(w, r)
			return
		}

		// get database client
		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		// get user from database
		usr, err := user.GetWithIDAndToken(dbClient, credentials.UserID, credentials.Token)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if usr == nil {
			respondWithError(http.StatusUnauthorized, "", w)
			return
		}

		// got user from database with valid credentials
		// add user to request context
		ctx := context.WithValue(r.Context(), CONTEXT_FIELD_USER, usr)

		// continue the request processing
		next.ServeHTTP(w, r.WithContext(ctx))
	})
}

// Expects user to be stored in context,
// Updates sender's LastSeen fiels.
func lastSeenMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		u, ok := r.Context().Value(CONTEXT_FIELD_USER).(*user.User)
		if ok == false || u == nil {
			// authentication handled by middleware
			// missing user here is an internal server error
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		dbClient, err := dbclient.SharedUserDB()
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		err = u.UpdateLastSeen(dbClient)
		if err != nil {
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}

		next.ServeHTTP(w, r)
	})
}

func cubzhClientVersionMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// parse HTTP header containing the Cubzh API client version
		clientVersion := r.Header.Get(HTTP_HEADER_CUBZH_APP_VERSION_V2)
		if len(clientVersion) == 0 {
			fmt.Println("⚠️ [Middleware] client version header missing", r.URL.Path)
		}

		// parse HTTP header containing the Cubzh API client version
		clientBuildnumber := r.Header.Get(HTTP_HEADER_CUBZH_APP_BUILDNUMBER_V2)
		if len(clientBuildnumber) == 0 {
			fmt.Println("⚠️ [Middleware] client buildnumber header missing", r.URL.Path)
		}

		buildNumber, err := strconv.Atoi(clientBuildnumber)
		if err != nil {
			fmt.Println("⚠️ [Middleware] client buildnumber not an integer", r.URL.Path, clientBuildnumber)
			buildNumber = 0
		}

		// Store client version in request object
		// create a new request context containing the data
		ctxWithValue := context.WithValue(r.Context(), REQ_CLIENTVERSION, clientVersion)
		ctxWithValue = context.WithValue(ctxWithValue, REQ_CLIENTBUILDNUMBER, buildNumber)
		// create a new request using that new context
		newReq := r.WithContext(ctxWithValue)

		// Call the next handler, which can be another middleware in the chain, or the final handler.
		next.ServeHTTP(w, newReq)
	})
}

func adminAuthMiddleware(next http.Handler) http.Handler {

	adminUserIDs := []string{
		"4d558bc1-5700-4a0d-8c68-f05e0b97f3fd", // aduermael
		"59991c61-cb96-4a11-adc6-452f58931bde", // gaetan
		"a7d0ecc6-f7ad-4630-92b2-c573682ceed7", // petroglyph
	}

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		// check identity of the user
		// and make sure it's an admin
		usr, err := getUserFromRequestContext(r)
		if err != nil {
			fmt.Println("[❌][ADMIN AUTH] failed to get user from request context")
			respondWithError(http.StatusInternalServerError, "", w)
			return
		}
		if !slices.Contains(adminUserIDs, usr.ID) {
			fmt.Println("[❌][ADMIN AUTH] user is not an admin")
			respondWithError(http.StatusUnauthorized, "", w)
			return
		}

		// TODO: check signature
		// // get path
		// path := r.URL.Path
		// // compute hash
		// hash := sha256.Sum256([]byte(path + "<|>" + r.Method + "&bs82;s"))
		// // get header "signature"
		// signatureHeader := r.Header.Get("Vx-Hash")
		// if signatureHeader != hex.EncodeToString(hash[:]) {
		// 	respondWithError(http.StatusForbidden, "", w)
		// 	return
		// }

		// continue
		next.ServeHTTP(w, r)
	})
}

// creates session and sets cookie
func newSession(w http.ResponseWriter, r *http.Request) (*sessionstore.Session, error) {

	// TODO: remove when 187 is not used anymore
	newPrivilegedToken, err := utils.UUID()
	if err != nil {
		return nil, err
	}

	session := sessionStore.New()
	session.Options.MaxAge = 60 * 60 * 24 * 30 // one month
	session.Values[SESSION_KEY_PRIVILEGED_TOKEN] = newPrivilegedToken

	err = sessionStore.Save(session) // we need to save it now for ID to be attributed
	if err != nil {
		fmt.Println("🍪❌ COULD NOT SAVE SESSION")
		return nil, err
	}

	// fmt.Println("🍪 CREATED NEW SESSION WITH ID:", session.ID)

	// Set "Set-Cookie" header (session)
	http.SetCookie(w, &http.Cookie{
		Domain:   "api.cu.bzh",
		Path:     "",
		Name:     HTTP_COOKIE_SESSION,
		Value:    session.ID,
		MaxAge:   session.Options.MaxAge,
		HttpOnly: true, // Makes the cookie inaccessible to JavaScript running in the browser
		Secure:   true,
	})

	return session, nil
}

// NOTE: what happens when handling more than one request at the same time
// from the same client? we could create several sessions for it, and that would be a mistake.
// (could also be enforced client side: no session == one request at a time)
func sessionMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {

		var session *sessionstore.Session
		var err error

		cookie, err := r.Cookie(HTTP_COOKIE_SESSION)
		if err != nil {
			if err != http.ErrNoCookie {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			// no cookie -> create session
			session, err = newSession(w, r)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

		} else {
			// the session cookie was found

			session, err = sessionStore.Get(cookie.Value)
			if err != nil {
				respondWithError(http.StatusInternalServerError, "", w)
				return
			}

			if session == nil { // not found, create a new one!
				session, err = newSession(w, r)
				if err != nil {
					respondWithError(http.StatusInternalServerError, "", w)
					return
				}
			} else {
				err = updateSession(session, &w, r)
				if err != nil {
					respondWithError(http.StatusInternalServerError, "", w)
					return
				}
				// fmt.Println("🍪 LOADED SESSION WITH ID:", session.ID)
			}
		}

		ctx := context.WithValue(r.Context(), CONTEXT_SESSION_KEY, session)

		next.ServeHTTP(w, r.WithContext(ctx))

		// Save session when done processing request
		// NOTES:
		// - needed at least to extend TTL
		// - we could only extend TTL if content didn't change
		err = sessionStore.Save(session)
		if err != nil {
			fmt.Println("❌ COULD NOT SAVE SESSION")
		}
	})
}

// / makes sure the session is up to date
// / ex: if the privileged token is missing, it adds it
func updateSession(s *sessionstore.Session, w *http.ResponseWriter, r *http.Request) error {
	if s == nil {
		return errors.New("session is nil")
	}
	if w == nil {
		return errors.New("response writer is nil")
	}

	// do stuff here

	// TODO: if session is close from expiration, we could extend it here

	return nil // success
}

// --------------------------------------------------
//
// Utility functions
//
// --------------------------------------------------

type ClientInfo struct {
	Version     string
	BuildNumber int
}

// getClientInfo returns the client version and build number from the request context
func getClientInfo(r *http.Request) (ClientInfo, error) {
	clientVersion, err := getClientVersionFromRequestContext(r)
	if err != nil {
		return ClientInfo{}, err
	}

	clientBuildNumber, err := getClientBuildnumberFromRequestContext(r)
	if err != nil {
		return ClientInfo{}, err
	}

	return ClientInfo{
		Version:     clientVersion,
		BuildNumber: clientBuildNumber,
	}, nil
}

// How to use :
//
// clientVersion, err := getClientVersionFromRequestContext(r)
//
//	if err != nil {
//		fmt.Println("❌ DELETE /itemdrafts/{id} missing version info:", err.Error())
//		respondWithError(http.StatusBadRequest, "no client version", w)
//		return
//	}
func getClientVersionFromRequestContext(r *http.Request) (string, error) {
	if r == nil {
		return "", errors.New("request argument is nil")
	}
	value, ok := (r.Context().Value(REQ_CLIENTVERSION)).(string)
	if !ok {
		return "", errors.New("field not found in context")
	}
	return value, nil
}

// How to use :
//
// clientBuildNumber, err := getClientBuildnumberFromRequestContext(r)
//
//	if err != nil {
//		fmt.Println("❌ DELETE /itemdrafts/{id} missing buildnumber info:", err.Error())
//		respondWithError(http.StatusBadRequest, "no client build number", w)
//		return
//	}
func getClientBuildnumberFromRequestContext(r *http.Request) (int, error) {
	if r == nil {
		return 0, errors.New("request argument is nil")
	}
	value, ok := (r.Context().Value(REQ_CLIENTBUILDNUMBER)).(int)
	if !ok {
		return 0, errors.New("field not found in context")
	}
	return value, nil
}

// getUserFromRequestContext returns user stored in context
func getUserFromRequestContext(r *http.Request) (*user.User, error) {
	if r == nil {
		return nil, errors.New("request argument is nil")
	}
	user, ok := (r.Context().Value(CONTEXT_FIELD_USER)).(*user.User)
	if !ok {
		return nil, errors.New("field not found in context")
	}
	return user, nil
}

// getSessionFromRequestContext returns session stored in context
func getSessionFromRequestContext(r *http.Request) (*sessionstore.Session, error) {
	if r == nil {
		return nil, errors.New("request argument is nil")
	}
	session, ok := (r.Context().Value(CONTEXT_SESSION_KEY)).(*sessionstore.Session)
	if !ok {
		return nil, errors.New("field not found in context")
	}
	return session, nil
}
