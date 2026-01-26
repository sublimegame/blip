#!/bin/bash

set -e

# Update the nginx configuration on the server
scp -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes ./nginx.conf ubuntu@15.235.116.53:/voxowl/nginx/nginx.conf

# Restart the nginx service
DOCKER_SERVICE_NAME="nginx-for-services"
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes "docker service update --force ${DOCKER_SERVICE_NAME}"
