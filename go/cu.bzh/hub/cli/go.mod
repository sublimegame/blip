module cu.bzh/hub/cli

go 1.24.4

replace github.com/gocql/gocql => github.com/scylladb/gocql v1.15.1

replace cu.bzh/utils v0.0.1 => ../../utils

replace cu.bzh/mailer v0.0.1 => ../../mailer

replace cu.bzh/hub/game v0.0.1 => ../game

replace cu.bzh/hub/search v0.0.1 => ../search

replace cu.bzh/hub/dbclient v0.0.1 => ../dbclient

replace cu.bzh/hub/user v0.0.1 => ../user

replace cu.bzh/hub/types v0.0.1 => ../types

replace cu.bzh/hub/item v0.0.1 => ../item

replace cu.bzh/hub/fcmclient v0.0.1 => ../fcmclient

replace cu.bzh/hub/scyllaclient v0.0.1 => ../scyllaclient

replace cu.bzh/hub/scyllaclient/model/transaction v0.0.1 => ../scyllaclient/model/transaction

replace cu.bzh/hub/scyllaclient/model/notification v0.0.1 => ../scyllaclient/model/notification

replace cu.bzh/hub/scyllaclient/model/kvs v0.0.1 => ../scyllaclient/model/kvs

replace cu.bzh/hub/scyllaclient/model/universe v0.0.1 => ../scyllaclient/model/universe

replace cu.bzh/hub/apnsclient v0.0.1 => ../apnsclient

replace cu.bzh/scheduler/types v0.0.1 => ../../scheduler/types

replace cu.bzh/types/gameserver v0.0.1 => ../../types/gameserver

require (
	cu.bzh/hub/dbclient v0.0.1
	cu.bzh/hub/game v0.0.1
	cu.bzh/hub/item v0.0.1
	cu.bzh/hub/scyllaclient v0.0.1
	cu.bzh/hub/search v0.0.1
	cu.bzh/hub/user v0.0.1
	cu.bzh/mailer v0.0.1
	github.com/urfave/cli v1.22.16
)

require (
	cu.bzh/hub/scyllaclient/model/kvs v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/notification v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/transaction v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/universe v0.0.1 // indirect
	cu.bzh/hub/types v0.0.1 // indirect
	cu.bzh/scheduler/types v0.0.1 // indirect
	cu.bzh/types/gameserver v0.0.1 // indirect
	cu.bzh/utils v0.0.1 // indirect
	github.com/PuerkitoBio/goquery v1.6.2-0.20210118231321-bedb4e7a8891 // indirect
	github.com/andybalholm/cascadia v1.2.0 // indirect
	github.com/apapsch/go-jsonmerge/v2 v2.0.0 // indirect
	github.com/cpuguy83/go-md2man/v2 v2.0.5 // indirect
	github.com/deepmap/oapi-codegen v1.12.3 // indirect
	github.com/gdevillele/frontparser v0.0.0-20161110013724-f28e87c7b9da // indirect
	github.com/gocql/gocql v1.7.0 // indirect
	github.com/golang/snappy v0.0.4 // indirect
	github.com/gomodule/redigo v1.9.2 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/gorilla/css v1.0.1-0.20190627041119-4940b8d78103 // indirect
	github.com/klauspost/compress v1.17.9 // indirect
	github.com/mailjet/mailjet-apiv3-go v0.0.0-20170621082554-4597cb4aae12 // indirect
	github.com/montanaflynn/stats v0.0.0-20171201202039-1bf9dbcd8cbe // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/russross/blackfriday/v2 v2.1.0 // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	github.com/scylladb/gocqlx/v3 v3.0.2 // indirect
	github.com/sony/gobreaker v0.5.0 // indirect
	github.com/typesense/typesense-go v1.0.0 // indirect
	github.com/vanng822/css v0.1.0 // indirect
	github.com/vanng822/go-premailer v1.9.0 // indirect
	github.com/xdg-go/pbkdf2 v1.0.0 // indirect
	github.com/xdg-go/scram v1.1.1 // indirect
	github.com/xdg-go/stringprep v1.0.3 // indirect
	github.com/youmark/pkcs8 v0.0.0-20181117223130-1be2e3e5546d // indirect
	go.mongodb.org/mongo-driver v1.11.9 // indirect
	golang.org/x/crypto v0.18.0 // indirect
	golang.org/x/net v0.20.0 // indirect
	golang.org/x/sync v0.11.0 // indirect
	golang.org/x/text v0.14.0 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
)
