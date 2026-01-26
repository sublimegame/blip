package sessionstore

import (
	"bytes"
	"encoding/base32"
	"encoding/gob"
	"errors"
	"strings"
	"time"

	"github.com/gomodule/redigo/redis"
	"github.com/gorilla/securecookie"
)

// NOTE: aduermael:
// inspired/copied from https://github.com/boj/SessionStore/blob/v1.2/SessionStore.go
const (
	DEFAULT_MAX_AGE = 86400 * 30 // a month
)

// Options stores configuration for a session or session store.
type Options struct {
	// MaxAge=0 means no Max-Age attribute specified and the cookie will be
	// deleted after the browser session ends.
	// MaxAge<0 means delete cookie immediately.
	// MaxAge>0 means Max-Age attribute present and given in seconds.
	MaxAge int
}

// Session ...
type Session struct {
	ID      string
	Values  map[interface{}]interface{}
	Options *Options
}

// SessionSerializer provides an interface hook for alternative serializers
type SessionSerializer interface {
	Deserialize(d []byte, ss *Session) error
	Serialize(ss *Session) ([]byte, error)
}

// GobSerializer uses gob package to encode the session map
type GobSerializer struct{}

// Serialize using gob
func (s GobSerializer) Serialize(ss *Session) ([]byte, error) {
	buf := new(bytes.Buffer)
	enc := gob.NewEncoder(buf)
	err := enc.Encode(ss.Values)
	if err == nil {
		return buf.Bytes(), nil
	}
	return nil, err
}

// Deserialize back to map[interface{}]interface{}
func (s GobSerializer) Deserialize(d []byte, ss *Session) error {
	dec := gob.NewDecoder(bytes.NewBuffer(d))
	return dec.Decode(&ss.Values)
}

// SessionStore stores sessions in a redis backend.
type SessionStore struct {
	Pool       *redis.Pool
	Options    *Options // default Options for sessions
	maxLength  int
	keyPrefix  string
	serializer SessionSerializer
}

// SetMaxLength sets SessionStore.maxLength if the `l` argument is greater or equal 0
// maxLength restricts the maximum length of new sessions to l.
// If l is 0 there is no limit to the size of a session, use with caution.
// The default for a new SessionStore is 4096. Redis allows for max.
// value sizes of up to 512MB (http://redis.io/topics/data-types)
// Default: 4096,
func (s *SessionStore) SetMaxLength(l int) {
	if l >= 0 {
		s.maxLength = l
	}
}

// SetKeyPrefix set the prefix
func (s *SessionStore) SetKeyPrefix(p string) {
	s.keyPrefix = p
}

// SetSerializer sets the serializer
func (s *SessionStore) SetSerializer(ss SessionSerializer) {
	s.serializer = ss
}

// SetMaxAge restricts the maximum age, in seconds
func (s *SessionStore) SetMaxAge(v int) {
	s.Options.MaxAge = v
}

func dial(network, address, password string) (redis.Conn, error) {
	c, err := redis.Dial(network, address)
	if err != nil {
		return nil, err
	}
	if password != "" {
		if _, err := c.Do("AUTH", password); err != nil {
			c.Close()
			return nil, err
		}
	}
	return c, err
}

func newSessionStoreWithPool(pool *redis.Pool) (*SessionStore, error) {
	rs := &SessionStore{
		// http://godoc.org/github.com/gomodule/redigo/redis#Pool
		Pool: pool,
		Options: &Options{
			MaxAge: DEFAULT_MAX_AGE,
		},
		maxLength:  4096,
		keyPrefix:  "s_",
		serializer: GobSerializer{},
	}
	_, err := rs.ping()
	return rs, err
}

// NewSessionStore returns a new SessionStore.
// size: maximum number of idle connections.
func NewSessionStore(size int, network, address, password string) (*SessionStore, error) {
	return newSessionStoreWithPool(&redis.Pool{
		MaxIdle:     size,
		IdleTimeout: 240 * time.Second,
		TestOnBorrow: func(c redis.Conn, t time.Time) error {
			_, err := c.Do("PING")
			return err
		},
		Dial: func() (redis.Conn, error) {
			return dial(network, address, password)
		},
	})
}

// Close closes the underlying *redis.Pool
func (s *SessionStore) Close() error {
	return s.Pool.Close()
}

// Get returns a session for the given sessionID
func (s *SessionStore) Get(sessionID string) (*Session, error) {
	var err error

	session := &Session{
		ID:      sessionID,
		Values:  make(map[interface{}]interface{}),
		Options: new(Options),
	}

	found, err := s.load(session)
	if err != nil {
		return nil, err
	}

	if found == false {
		return nil, nil
	}

	return session, nil
}

// New returns a new session, not yet stored in the redis database
func (s *SessionStore) New() *Session {
	return &Session{
		ID:      "",
		Values:  make(map[interface{}]interface{}),
		Options: new(Options),
	}
}

// Save adds a single session to the response.
func (s *SessionStore) Save(session *Session) error {
	// Marked for deletion.
	if session.Options.MaxAge < 0 {
		if err := s.delete(session); err != nil {
			return err
		}
	} else {
		// Build an alphanumeric key for the redis store.
		if session.ID == "" {
			session.ID = strings.TrimRight(base32.StdEncoding.EncodeToString(securecookie.GenerateRandomKey(32)), "=")
		}
		if err := s.save(session); err != nil {
			return err
		}
	}
	return nil
}

// Delete removes the session from redis, and sets the cookie to expire.
func (s *SessionStore) Delete(session *Session) error {
	conn := s.Pool.Get()
	defer conn.Close()
	if _, err := conn.Do("DEL", s.keyPrefix+session.ID); err != nil {
		return err
	}
	session.Options.MaxAge = -1
	// Clear session values.
	for k := range session.Values {
		delete(session.Values, k)
	}
	return nil
}

// ping does an internal ping against a server to check if it is alive.
func (s *SessionStore) ping() (bool, error) {
	conn := s.Pool.Get()
	defer conn.Close()
	data, err := conn.Do("PING")
	if err != nil || data == nil {
		return false, err
	}
	return (data == "PONG"), nil
}

// save stores the session in redis.
func (s *SessionStore) save(session *Session) error {
	b, err := s.serializer.Serialize(session)
	if err != nil {
		return err
	}
	if s.maxLength != 0 && len(b) > s.maxLength {
		return errors.New("SessionStore: the value to store is too big")
	}
	conn := s.Pool.Get()
	defer conn.Close()
	if err = conn.Err(); err != nil {
		return err
	}
	age := session.Options.MaxAge
	if age == 0 {
		age = s.Options.MaxAge
	}
	_, err = conn.Do("SETEX", s.keyPrefix+session.ID, age, b)
	return err
}

// load reads the session from redis.
// returns true if there is a session data in DB
func (s *SessionStore) load(session *Session) (bool, error) {
	conn := s.Pool.Get()
	defer conn.Close()
	if err := conn.Err(); err != nil {
		return false, err
	}
	data, err := conn.Do("GET", s.keyPrefix+session.ID)
	if err != nil {
		return false, err
	}
	if data == nil {
		return false, nil // no data was associated with this key
	}
	b, err := redis.Bytes(data, err)
	if err != nil {
		return false, err
	}
	return true, s.serializer.Deserialize(b, session)
}

// delete removes keys from redis if MaxAge<0
func (s *SessionStore) delete(session *Session) error {
	conn := s.Pool.Get()
	defer conn.Close()
	if _, err := conn.Do("DEL", s.keyPrefix+session.ID); err != nil {
		return err
	}
	return nil
}
