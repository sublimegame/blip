#!/bin/bash

# cleanup, making sure old docker compose project isn't running
# docker ps -aq --filter="label=com.docker.compose.project=particubes-worlds" | xargs docker rm -f
# docker ps -aq --filter="network=particubes-worlds_default" | xargs docker rm -f
# docker network rm particubes-worlds_default &> /dev/null

set -e

START_LOCATION="$PWD"
SCRIPT_LOCATION=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# Go back to start location when script exits
trap "cd $START_LOCATION" EXIT
# Go to script location before running git command
# to make sure it runs within project tree
cd "$SCRIPT_LOCATION"
# Use git command to get root project directory.
PROJECT_ROOT=$(git rev-parse --show-toplevel)
# The script is now executed from project root directory
cd "$PROJECT_ROOT"

# ====================
# CONFIG.JSON
# ====================
CONFIG_JSON="$PROJECT_ROOT/common/assets/config.json"

# ====================
# PRODUCTION CONFIG
# ====================

# public IP of a Swarm manager node
SWARM_MANAGER_IP="52.14.86.79"

# ====================
# KVS (Key-Value Store)
# ====================
# HUB_KVS_IMAGE_NAME="czh_hub_kvs"
# HUB_KVS_CONTAINER_NAME="czh_hub_kvs"

IP=$(ifconfig | grep "inet " | grep -Fv 127.0.0.1 | awk '{print $2}')
IP=$( cut -d $' ' -f 1 <<< $IP )

MODE=""

BACKUP_DATE="2024-07-11"
BACKUP_TAR_GZ="servers/backups/$BACKUP_DATE.tar.gz"

function installBackupFiles {

	rm -rf servers/debug/*

	tar -zxvf "servers/backups/$BACKUP_DATE.tar.gz" -C "servers/debug/"

	mv "servers/debug/$BACKUP_DATE/particubes_games" "servers/debug/particubes_games"
	mv "servers/debug/$BACKUP_DATE/particubes_items" "servers/debug/particubes_items"
	mv "servers/debug/$BACKUP_DATE/dump.tar.gz" "servers/debug/dump.tar.gz"

	rm -r "servers/debug/$BACKUP_DATE"

	tar -zxvf "servers/debug/dump.tar.gz" -C "servers/debug"

	rm "servers/debug/dump.tar.gz"
}

while read -p "dev-swarm, dev-data-store, stop-swarm, prod-all, prod-website, prod-hub, prod-schedulers, prod-tracking, prod-mongo-express, wasm-prod, dashboard, dashboard-dev, discordbot, ping-prod, games-kvs-prod, prod-link-service, prod-parent-dashboard, nginx-web, fileserver, scylladb ? " v; do
    if [ "$v" = "dev-swarm" ] || [ "$v" = "dev-data-store" ] || [ "$v" = "stop-swarm" ] || [ "$v" = "prod-all" ] ||
    	[ "$v" = "prod-website" ] || [ "$v" = "prod-hub" ] || [ "$v" = "prod-schedulers" ] || [ "$v" = "prod-tracking" ] ||
		[ "$v" = "prod-mongo-express" ] || [ "$v" = "wasm-prod" ] || [ "$v" = "dashboard" ] || [ "$v" = "dashboard-dev" ] ||
		[ "$v" = "discordbot" ] || [ "$v" = "ping-prod" ] || [ "$v" = "games-kvs-prod" ] || [ "$v" = "prod-link-service" ] ||
		[ "$v" = "prod-parent-dashboard" ] || [ "$v" = "nginx-web" ] || [ "$v" = "fileserver" ] || [ "$v" = "scylladb" ]
    then
        MODE=$v
        break
    fi
    echo "option not supported"
done

if [ "$MODE" = "dev-swarm" ]
then
	installBackupFiles

	# UPDATE CONFIG.JSON
	docker run --rm -i -v "$CONFIG_JSON":/config.json imega/jq ".APIHost = \"http://$IP:10083\"" /config.json \
	> "$CONFIG_JSON-new"

	mv "$CONFIG_JSON-new" "$CONFIG_JSON"

	# install all labels on local node
	NODE_ID=$(docker node ls | grep "*" | awk 'END {print $1}')
	docker node update --label-add tracking=1 --label-add master=1 --label-add websites=1 --label-add gameservers-eu=1 "$NODE_ID"

	HUB_KVS_PUBLIC_IP=$IP docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod-local.yml build

	HUB_KVS_PUBLIC_IP=$IP docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod-local.yml pcubes

	echo "waiting for services to start..."
	sleep 10 # NOTE: a while loop checking for container presence would be better

	USER_DB_CONTAINER_ID=$(docker ps | grep pcubes_user-db | awk 'END {print $1}')

	echo "USER_DB_CONTAINER_ID: $USER_DB_CONTAINER_ID"

	set +e
	docker exec -ti $USER_DB_CONTAINER_ID mongorestore /dump &> /dev/null
	set -e

	HUB_CONTAINER_ID=$(docker ps | grep pcubes_hub-https | awk 'END {print $1}')
	WEB_CONTAINER_ID=$(docker ps | grep pcubes_web | awk 'END {print $1}')

	echo "HUB_CONTAINER_ID: $HUB_CONTAINER_ID"
	echo "WEB_CONTAINER_ID: $WEB_CONTAINER_ID"

	echo "-----------------------"
	echo "-- HUB ----------------"
	echo "# attach to container:"
	echo "docker exec -ti $HUB_CONTAINER_ID bash"
	echo "# run server:"
	echo "go run *.go hub:10080"
	echo "# run cli:"
	echo "cd cli"
	echo "go run *.go"
	echo "-- WEBSITE ------------"
	echo "# attach to container:"
	echo "docker exec -ti $WEB_CONTAINER_ID ash"
	echo "# run server:"
	echo "GO111MODULE=off go run *.go"
	echo "-----------------------"

elif [ "$MODE" = "dev-data-store" ]
then
	start_local_kvs

elif [ "$MODE" = "prod-all" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "prod-website" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build web
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push web
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "prod-hub" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build hub-https
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push hub-https
		# no need to build/push hub-secure images, it's the same.
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "fileserver" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build fileserver
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push fileserver
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "prod-link-service" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build link-service
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push link-service
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "wasm-prod" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build wasm
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push wasm
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes
	# TODO: (gdevillele) here, we might want to automate the invalidation of Cloudflare cache for `app.cu.bzh` subdomain.
	#                    It's not urgent at all though.

elif [ "$MODE" = "prod-parent-dashboard" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build parentdashboard
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push parentdashboard
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "nginx-web" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build nginx-web
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push nginx-web
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "scylladb" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build scylladb
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push scylladb
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "discordbot" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build discordbot
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push discordbot
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "ping-prod" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build ping nginx-servers
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push ping nginx-servers
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "prod-schedulers" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build gameservers-eu
		DOCKER_DEFAULT_PLATFORM=linux/amd64 COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push gameservers-eu
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "prod-tracking" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose-prod-tracking.yml build
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose-prod-tracking.yml push
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose-prod-tracking.yml pcubes

elif [ "$MODE" = "prod-mongo-express" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose-prod-db-admin.yml build
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose-prod-db-admin.yml push
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose-prod-db-admin.yml pcubes

elif [ "$MODE" = "games-kvs-prod" ]
then
	# 18.190.42.21 is the IP address of the swarm manager
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

elif [ "$MODE" = "stop-swarm" ]
then
	docker stack rm pcubes

elif [ "$MODE" = "dashboard-dev" ]
then
	echo "DASHBOARD DEV"
	docker build \
	--target build-env \
	-t cubzh_dashboard \
	-f "dockerfiles/dashboard.Dockerfile" .

elif [ "$MODE" = "dashboard" ]
then
	if [ "$DOCKER_HOST" = "" ]; then
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml build dashboard
		COMPOSE_DOCKER_CLI_BUILD=1 DOCKER_BUILDKIT=1 docker compose -f servers/docker-compose.yml -f servers/docker-compose-prod.yml push dashboard
	else
		echo "❌ this command is intended to run on local environment (one docker machine is currently active)"
	fi
	DOCKER_HOST=ssh://ubuntu@${SWARM_MANAGER_IP} \
	docker stack deploy --with-registry-auth --compose-file servers/docker-compose.yml --compose-file servers/docker-compose-prod.yml pcubes

fi
