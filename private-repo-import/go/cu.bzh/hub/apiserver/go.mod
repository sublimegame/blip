module cu.bzh/hub/apiserver

go 1.24.4

replace github.com/gocql/gocql => github.com/scylladb/gocql v1.15.1

replace cu.bzh/utils v0.0.1 => ../../utils

replace cu.bzh/cors v0.0.1 => ../../cors

replace cu.bzh/ggwp v0.0.1 => ../../ggwp

replace cu.bzh/mailer v0.0.1 => ../../mailer

replace cu.bzh/sessionstore v0.0.1 => ../../sessionstore

replace cu.bzh/scheduler/types v0.0.1 => ../../scheduler/types

replace cu.bzh/hub/game v0.0.1 => ../game

replace cu.bzh/hub/search v0.0.1 => ../search

replace cu.bzh/hub/user v0.0.1 => ../user

replace cu.bzh/hub/item v0.0.1 => ../item

replace cu.bzh/hub/cli v0.0.1 => ../cli

replace cu.bzh/hub/dbclient v0.0.1 => ../dbclient

replace cu.bzh/hub/types v0.0.1 => ../types

replace cu.bzh/hub/scyllaclient v0.0.1 => ../scyllaclient

replace cu.bzh/hub/scyllaclient/model/transaction v0.0.1 => ../scyllaclient/model/transaction

replace cu.bzh/hub/scyllaclient/model/notification v0.0.1 => ../scyllaclient/model/notification

replace cu.bzh/hub/scyllaclient/model/kvs v0.0.1 => ../scyllaclient/model/kvs

replace cu.bzh/hub/scyllaclient/model/universe v0.0.1 => ../scyllaclient/model/universe

replace cu.bzh/hub/fcmclient v0.0.1 => ../fcmclient

replace cu.bzh/hub/apnsclient v0.0.1 => ../apnsclient

replace cu.bzh/types/gameserver v0.0.1 => ../../types/gameserver

replace cu.bzh/ai/moderation v0.0.1 => ../../ai/moderation

replace cu.bzh/utils/http v0.0.1 => ../../utils/http

replace cu.bzh/hub/transactions v0.0.1 => ../transactions

replace blip.game/git v0.0.1 => ../../../blip.game/git

replace cu.bzh/ai => ../../ai

replace cu.bzh/ai/anthropic => ../../ai/anthropic

replace blip.game/discord => ../../../blip.game/discord

replace blip.game/middleware => ../../../blip.game/middleware

require (
	blip.game/discord v0.0.0-00010101000000-000000000000
	blip.game/git v0.0.1
	blip.game/middleware v0.0.0-00010101000000-000000000000
	cu.bzh/ai v0.0.0-00010101000000-000000000000
	cu.bzh/ai/anthropic v0.0.0-00010101000000-000000000000
	cu.bzh/ai/moderation v0.0.1
	cu.bzh/cors v0.0.1
	cu.bzh/ggwp v0.0.1
	cu.bzh/hub/apnsclient v0.0.1
	cu.bzh/hub/dbclient v0.0.1
	cu.bzh/hub/fcmclient v0.0.1
	cu.bzh/hub/game v0.0.1
	cu.bzh/hub/item v0.0.1
	cu.bzh/hub/scyllaclient v0.0.1
	cu.bzh/hub/scyllaclient/model/kvs v0.0.1
	cu.bzh/hub/search v0.0.1
	cu.bzh/hub/transactions v0.0.1
	cu.bzh/hub/types v0.0.1
	cu.bzh/hub/user v0.0.1
	cu.bzh/mailer v0.0.1
	cu.bzh/scheduler/types v0.0.1
	cu.bzh/sessionstore v0.0.1
	cu.bzh/types/gameserver v0.0.1
	cu.bzh/utils v0.0.1
	cu.bzh/utils/http v0.0.1
	github.com/andybalholm/brotli v1.2.0
	github.com/ding-live/ding-golang v0.16.51
	github.com/fxamacker/cbor v1.5.1
	github.com/go-chi/chi/v5 v5.2.1
	github.com/golang-jwt/jwt/v4 v4.5.0
	github.com/google/uuid v1.6.0
	github.com/qmuntal/gltf v0.28.0
	github.com/sergi/go-diff v1.3.2-0.20230802210424-5b0b94c5c0d3
	github.com/twilio/twilio-go v1.24.1
	golang.org/x/image v0.25.0
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
	cu.bzh/hub/scyllaclient/model/notification v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/transaction v0.0.1 // indirect
	cu.bzh/hub/scyllaclient/model/universe v0.0.1 // indirect
	dario.cat/mergo v1.0.0 // indirect
	firebase.google.com/go/v4 v4.14.1 // indirect
	github.com/MicahParks/keyfunc v1.9.0 // indirect
	github.com/Microsoft/go-winio v0.6.2 // indirect
	github.com/ProtonMail/go-crypto v1.1.5 // indirect
	github.com/PuerkitoBio/goquery v1.6.2-0.20210118231321-bedb4e7a8891 // indirect
	github.com/andybalholm/cascadia v1.2.0 // indirect
	github.com/apapsch/go-jsonmerge/v2 v2.0.0 // indirect
	github.com/bwmarrin/discordgo v0.28.1 // indirect
	github.com/cloudflare/circl v1.6.0 // indirect
	github.com/cyphar/filepath-securejoin v0.4.1 // indirect
	github.com/deepmap/oapi-codegen v1.12.3 // indirect
	github.com/emirpasic/gods v1.18.1 // indirect
	github.com/ericlagergren/decimal v0.0.0-20221120152707-495c53812d05 // indirect
	github.com/felixge/httpsnoop v1.0.4 // indirect
	github.com/gdevillele/frontparser v0.0.0-20161110013724-f28e87c7b9da // indirect
	github.com/go-chi/cors v1.2.1 // indirect
	github.com/go-git/gcfg v1.5.1-0.20230307220236-3a3c6141e376 // indirect
	github.com/go-git/go-billy/v5 v5.6.2 // indirect
	github.com/go-git/go-git/v5 v5.14.0 // indirect
	github.com/go-logr/logr v1.4.2 // indirect
	github.com/go-logr/stdr v1.2.2 // indirect
	github.com/gocql/gocql v1.7.0 // indirect
	github.com/golang/groupcache v0.0.0-20241129210726-2c02b8208cf8 // indirect
	github.com/golang/mock v1.6.0 // indirect
	github.com/golang/protobuf v1.5.4 // indirect
	github.com/golang/snappy v0.0.4 // indirect
	github.com/gomodule/redigo v1.9.2 // indirect
	github.com/google/s2a-go v0.1.8 // indirect
	github.com/googleapis/enterprise-certificate-proxy v0.3.4 // indirect
	github.com/googleapis/gax-go/v2 v2.13.0 // indirect
	github.com/gorilla/css v1.0.1-0.20190627041119-4940b8d78103 // indirect
	github.com/gorilla/securecookie v1.1.2-0.20191028042304-61b4ad17eb88 // indirect
	github.com/gorilla/websocket v1.4.2 // indirect
	github.com/jbenet/go-context v0.0.0-20150711004518-d14ea06fba99 // indirect
	github.com/kevinburke/ssh_config v1.2.0 // indirect
	github.com/klauspost/compress v1.17.9 // indirect
	github.com/mailjet/mailjet-apiv3-go v0.0.0-20170621082554-4597cb4aae12 // indirect
	github.com/montanaflynn/stats v0.0.0-20171201202039-1bf9dbcd8cbe // indirect
	github.com/pjbgf/sha1cd v0.3.2 // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/russross/blackfriday/v2 v2.1.0 // indirect
	github.com/scylladb/go-reflectx v1.0.1 // indirect
	github.com/scylladb/gocqlx/v3 v3.0.2 // indirect
	github.com/sideshow/apns2 v0.24.0 // indirect
	github.com/skeema/knownhosts v1.3.1 // indirect
	github.com/sony/gobreaker v0.5.0 // indirect
	github.com/typesense/typesense-go v1.0.0 // indirect
	github.com/vanng822/css v0.1.0 // indirect
	github.com/vanng822/go-premailer v1.9.0 // indirect
	github.com/x448/float16 v0.8.4 // indirect
	github.com/xanzy/ssh-agent v0.3.3 // indirect
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
	golang.org/x/crypto v0.35.0 // indirect
	golang.org/x/exp v0.0.0-20250305212735-054e65f0b394 // indirect
	golang.org/x/net v0.35.0 // indirect
	golang.org/x/oauth2 v0.23.0 // indirect
	golang.org/x/sync v0.12.0 // indirect
	golang.org/x/sys v0.30.0 // indirect
	golang.org/x/text v0.23.0 // indirect
	golang.org/x/time v0.6.0 // indirect
	google.golang.org/api v0.199.0 // indirect
	google.golang.org/appengine/v2 v2.0.2 // indirect
	google.golang.org/genproto v0.0.0-20240903143218-8af14fe29dc1 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20240827150818-7e3bb234dfed // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240903143218-8af14fe29dc1 // indirect
	google.golang.org/grpc v1.67.0 // indirect
	google.golang.org/protobuf v1.34.2 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
	gopkg.in/warnings.v0 v0.1.2 // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
)
