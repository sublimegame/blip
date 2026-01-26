#!/bin/bash

# Check if correct number of arguments is provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <remote_ip> <username> <path_to_private_key>"
    echo "Example: $0 192.168.1.100 ubuntu ~/.ssh/my_key.pem"
    exit 1
fi

# gameservers-eu-1
# 141.94.97.66 ubuntu ~/.ssh/id_ovh_voxowl_com

# gameservers-us-1
# 51.222.244.45 ubuntu ~/.ssh/id_ovh_voxowl_com

# gameservers-sg-1
# 15.235.181.168 ubuntu ~/.ssh/id_ovh_voxowl_com

REMOTE_IP=$1
USERNAME=$2
PRIVATE_KEY=$3

# Check if private key file exists
if [ ! -f "$PRIVATE_KEY" ]; then
    echo "Error: Private key file not found at $PRIVATE_KEY"
    exit 1
fi

# SSH command prefix with key
SSH_BASE="ssh -i $PRIVATE_KEY -o IdentitiesOnly=yes $USERNAME@$REMOTE_IP"

echo "Connecting to $USERNAME@$REMOTE_IP..."

# Execute commands one by one
echo "⚙️ Enabling byobu..."
$SSH_BASE "byobu-enable"

echo "⚙️ Updating packages..."
$SSH_BASE "sudo apt update"

echo "⚙️ Upgrading packages..."
$SSH_BASE "sudo apt upgrade -y"

echo "⚙️ Installing docker..."
$SSH_BASE "sudo apt install -y docker.io"

echo "⚙️ Adding user to docker group..."
$SSH_BASE "sudo usermod -aG docker $USERNAME"

echo "⚙️ Rebooting server..."
$SSH_BASE "sudo reboot"

echo "Setup commands executed successfully."
echo "The remote server will reboot in 1 minute to apply changes."
echo "Please wait a few minutes before reconnecting."
