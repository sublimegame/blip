#!/bin/zsh

DOCKER_IMAGE_NAME='cubzh_windows_generate_icons'
PATH_GENERATE_ICONS='windows/generate-icons'
PATH_DOCKERFILE=${PATH_GENERATE_ICONS}'/Dockerfile'

# TODO: make sure we are at the root of the Git repo

# build docker image
docker build -t ${DOCKER_IMAGE_NAME} -f ${PATH_DOCKERFILE} ${PATH_GENERATE_ICONS}

# run docker images
docker run --rm -ti -v $(pwd)/$PATH_GENERATE_ICONS:/input -v $(pwd)/xptools/windows:/output ${DOCKER_IMAGE_NAME}

# remove docker image
docker image remove -f ${DOCKER_IMAGE_NAME}