module cu.bzh/hub/transactions

go 1.24.3

replace cu.bzh/hub/scyllaclient v0.0.1 => ../scyllaclient

replace cu.bzh/hub/scyllaclient/model/transaction v0.0.1 => ../scyllaclient/model/transaction

require cu.bzh/hub/scyllaclient v0.0.1

require (
	cu.bzh/hub/scyllaclient/model/transaction v0.0.1 // indirect
	github.com/gocql/gocql v0.0.0-20211015133455-b225f9b53fa1 // indirect
	github.com/golang/snappy v0.0.4 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/hailocab/go-hostpool v0.0.0-20160125115350-e80d13ce29ed // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	github.com/scylladb/gocqlx/v3 v3.0.1 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
)
