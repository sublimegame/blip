#!/bin/bash

set -e

# ssh root@67.205.183.56 -i ~/.ssh/id_rsa_cubzh_digitalocean -o IdentitiesOnly=yes

# Update the nginx configuration on the server
scp -i ~/.ssh/id_rsa_cubzh_digitalocean -o IdentitiesOnly=yes ./nginx.conf root@67.205.183.56:/voxowl/nginx/nginx.conf

# Restart the nginx service
DOCKER_SERVICE_NAME="nginx"
ssh root@67.205.183.56 -i ~/.ssh/id_rsa_cubzh_digitalocean -o IdentitiesOnly=yes "docker service update --force ${DOCKER_SERVICE_NAME}"
