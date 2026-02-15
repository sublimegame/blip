#!/bin/bash

#
# manager-2
#

set -e

# login with SSH key
ssh ubuntu@15.204.58.162 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
