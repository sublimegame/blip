module cu.bzh/hub/scyllaclient

go 1.24.4

// this is needed by github.com/scylladb/gocqlx/v3
replace github.com/gocql/gocql => github.com/scylladb/gocql v1.15.1

replace cu.bzh/hub/item v0.0.1 => ../item

replace cu.bzh/hub/user v0.0.1 => ../user

replace cu.bzh/utils v0.0.1 => ../../utils

replace cu.bzh/hub/scyllaclient/model/transaction v0.0.1 => ./model/transaction

replace cu.bzh/hub/scyllaclient/model/notification v0.0.1 => ./model/notification

replace cu.bzh/hub/scyllaclient/model/kvs v0.0.1 => ./model/kvs

replace cu.bzh/hub/scyllaclient/model/universe v0.0.1 => ./model/universe

require (
	cu.bzh/hub/scyllaclient/model/kvs v0.0.1
	cu.bzh/hub/scyllaclient/model/notification v0.0.1
	cu.bzh/hub/scyllaclient/model/transaction v0.0.1
	cu.bzh/hub/scyllaclient/model/universe v0.0.1
	github.com/gocql/gocql v1.7.0
	github.com/google/uuid v1.6.0
	github.com/scylladb/gocqlx/v3 v3.0.2
)

require (
	github.com/klauspost/compress v1.17.9 // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
)
