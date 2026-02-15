module cu.bzh/dashboard/legacy

go 1.23.3

replace cu.bzh/utils v0.0.1 => ../../utils

replace cu.bzh/hub/game v0.0.1 => ../../hub/game

replace cu.bzh/hub/item v0.0.1 => ../../hub/item

replace cu.bzh/scheduler/types v0.0.1 => ../../scheduler/types

replace cu.bzh/hub/dbclient v0.0.1 => ../../hub/dbclient

replace cu.bzh/hub/user v0.0.1 => ../../hub/user

replace cu.bzh/types/gameserver v0.0.1 => ../../types/gameserver

require (
	cu.bzh/hub/dbclient v0.0.1
	cu.bzh/hub/game v0.0.1
	cu.bzh/hub/item v0.0.1
	cu.bzh/hub/user v0.0.1
	github.com/go-chi/chi/v5 v5.1.0
)

require (
	cu.bzh/scheduler/types v0.0.1 // indirect
	cu.bzh/types/gameserver v0.0.1 // indirect
	cu.bzh/utils v0.0.1 // indirect
	github.com/golang/snappy v0.0.1 // indirect
	github.com/gomodule/redigo v1.8.9 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/klauspost/compress v1.13.6 // indirect
	github.com/montanaflynn/stats v0.0.0-20171201202039-1bf9dbcd8cbe // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/xdg-go/pbkdf2 v1.0.0 // indirect
	github.com/xdg-go/scram v1.1.1 // indirect
	github.com/xdg-go/stringprep v1.0.3 // indirect
	github.com/youmark/pkcs8 v0.0.0-20181117223130-1be2e3e5546d // indirect
	go.mongodb.org/mongo-driver v1.11.9 // indirect
	golang.org/x/crypto v0.0.0-20220622213112-05595931fe9d // indirect
	golang.org/x/sync v0.0.0-20210220032951-036812b2e83c // indirect
	golang.org/x/text v0.3.7 // indirect
)
