#!/bin/bash

#
# manager-1
#

set -e

# login with SSH key
ssh ubuntu@135.148.121.106 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
