module cu.bzh/hub/scyllaclient/model/notification

go 1.24.4

// this is needed by github.com/scylladb/gocqlx/v3
replace github.com/gocql/gocql => github.com/scylladb/gocql v1.15.1

require github.com/scylladb/gocqlx/v3 v3.0.2

require (
	github.com/gocql/gocql v1.7.0 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/klauspost/compress v1.17.9 // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
)
