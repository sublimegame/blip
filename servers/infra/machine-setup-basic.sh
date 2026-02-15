#!/bin/bash

byobu-enable

sudo apt update
sudo apt upgrade -y

# Install docker
sudo apt install -y docker.io

# You'll need to add your user `ubuntu` to the `docker` user group:
# (you need to reconnect your user for this to be taken into effect)
sudo usermod -aG docker ubuntu

sudo reboot
