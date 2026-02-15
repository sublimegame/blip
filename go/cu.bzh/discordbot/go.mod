module cu.bzh/discordbot

go 1.24.2

replace cu.bzh/hub/item v0.0.1 => ../hub/item

replace cu.bzh/hub/game v0.0.1 => ../hub/game

replace cu.bzh/hub/user v0.0.1 => ../hub/user

replace cu.bzh/hub/search v0.0.1 => ../hub/search

replace cu.bzh/utils v0.0.1 => ../utils

replace cu.bzh/scheduler/types v0.0.1 => ../scheduler/types

replace cu.bzh/types/gameserver v0.0.1 => ../types/gameserver

replace cu.bzh/web/link v0.0.1 => ../web/link

replace cu.bzh/amplitude v0.0.1 => ../amplitude

replace cu.bzh/hub/scyllaclient v0.0.1 => ../hub/scyllaclient

replace cu.bzh/hub/scyllaclient/model/transaction v0.0.1 => ../hub/scyllaclient/model/transaction

replace cu.bzh/hub/scyllaclient/model/notification v0.0.1 => ../hub/scyllaclient/model/notification

replace cu.bzh/hub/scyllaclient/model/kvs v0.0.1 => ../hub/scyllaclient/model/kvs

replace cu.bzh/hub/fcmclient v0.0.1 => ../hub/fcmclient

replace cu.bzh/hub/apnsclient v0.0.1 => ../hub/apnsclient

replace cu.bzh/hub/dbclient v0.0.1 => ../hub/dbclient

replace cu.bzh/hub/transactions v0.0.1 => ../hub/transactions

replace cu.bzh/hub/types v0.0.1 => ../hub/types

replace cu.bzh/utils/http v0.0.1 => ../utils/http

require (
	cu.bzh/hub/apnsclient v0.0.1
	cu.bzh/hub/dbclient v0.0.1
	cu.bzh/hub/fcmclient v0.0.1
	cu.bzh/hub/game v0.0.1
	cu.bzh/hub/item v0.0.1
	cu.bzh/hub/scyllaclient v0.0.1
	cu.bzh/hub/search v0.0.1
	cu.bzh/hub/transactions v0.0.1
	cu.bzh/hub/user v0.0.1
	cu.bzh/utils/http v0.0.1
	cu.bzh/web/link v0.0.1
	github.com/bwmarrin/discordgo v0.28.1
	github.com/go-chi/chi/v5 v5.2.1
	github.com/google/uuid v1.6.0
	github.com/gorilla/websocket v1.5.3
)

require (
	cloud.google.com/go v0.115.1 // indirect
	cloud.google.com/go/auth v0.9.5 // indirect
	cloud.google.com/go/auth/oauth2adapt v0.2.4 // indirect
	cloud.google.com/go/compute/metadata v0.5.2 // indirect
	cloud.google.com/go/firestore v1.16.0 // indirect
	cloud.google.com/go/iam v1.2.0 // indirect
	cloud.google.com/go/longrunning v0.6.0 // indirect
	cloud.google.com/go/storage v1.43.0 // indirect
	cu.bzh/hub/scyllaclient/model/kvs v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/notification v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/transaction v0.0.1 // indirect
	cu.bzh/hub/types v0.0.1 // indirect
	cu.bzh/scheduler/types v0.0.1 // indirect
	cu.bzh/types/gameserver v0.0.1 // indirect
	cu.bzh/utils v0.0.1 // indirect
	firebase.google.com/go/v4 v4.14.1 // indirect
	github.com/MicahParks/keyfunc v1.9.0 // indirect
	github.com/apapsch/go-jsonmerge/v2 v2.0.0 // indirect
	github.com/deepmap/oapi-codegen v1.12.3 // indirect
	github.com/felixge/httpsnoop v1.0.4 // indirect
	github.com/go-logr/logr v1.4.2 // indirect
	github.com/go-logr/stdr v1.2.2 // indirect
	github.com/gocql/gocql v1.7.0 // indirect
	github.com/golang-jwt/jwt/v4 v4.5.0 // indirect
	github.com/golang/groupcache v0.0.0-20210331224755-41bb18bfe9da // indirect
	github.com/golang/protobuf v1.5.4 // indirect
	github.com/golang/snappy v0.0.4 // indirect
	github.com/gomodule/redigo v1.9.2 // indirect
	github.com/google/s2a-go v0.1.8 // indirect
	github.com/googleapis/enterprise-certificate-proxy v0.3.4 // indirect
	github.com/googleapis/gax-go/v2 v2.13.0 // indirect
	github.com/hailocab/go-hostpool v0.0.0-20160125115350-e80d13ce29ed // indirect
	github.com/klauspost/compress v1.13.6 // indirect
	github.com/montanaflynn/stats v0.0.0-20171201202039-1bf9dbcd8cbe // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	github.com/scylladb/gocqlx/v3 v3.0.1 // indirect
	github.com/sideshow/apns2 v0.24.0 // indirect
	github.com/sony/gobreaker v0.5.0 // indirect
	github.com/typesense/typesense-go v1.0.0 // indirect
	github.com/xdg-go/pbkdf2 v1.0.0 // indirect
	github.com/xdg-go/scram v1.1.1 // indirect
	github.com/xdg-go/stringprep v1.0.3 // indirect
	github.com/youmark/pkcs8 v0.0.0-20181117223130-1be2e3e5546d // indirect
	go.mongodb.org/mongo-driver v1.11.9 // indirect
	go.opencensus.io v0.24.0 // indirect
	go.opentelemetry.io/contrib/instrumentation/google.golang.org/grpc/otelgrpc v0.54.0 // indirect
	go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp v0.54.0 // indirect
	go.opentelemetry.io/otel v1.29.0 // indirect
	go.opentelemetry.io/otel/metric v1.29.0 // indirect
	go.opentelemetry.io/otel/trace v1.29.0 // indirect
	golang.org/x/crypto v0.27.0 // indirect
	golang.org/x/net v0.29.0 // indirect
	golang.org/x/oauth2 v0.23.0 // indirect
	golang.org/x/sync v0.8.0 // indirect
	golang.org/x/sys v0.25.0 // indirect
	golang.org/x/text v0.18.0 // indirect
	golang.org/x/time v0.6.0 // indirect
	google.golang.org/api v0.199.0 // indirect
	google.golang.org/appengine/v2 v2.0.2 // indirect
	google.golang.org/genproto v0.0.0-20240903143218-8af14fe29dc1 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20240827150818-7e3bb234dfed // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240903143218-8af14fe29dc1 // indirect
	google.golang.org/grpc v1.67.0 // indirect
	google.golang.org/protobuf v1.34.2 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
)
