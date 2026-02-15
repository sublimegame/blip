#!/bin/bash

#
# gameservers-eu-1
#

set -e

# login with SSH key
ssh ubuntu@141.94.97.66 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
