#!/bin/bash

#
# gameservers-us-1
#

set -e

# login with SSH key
ssh ubuntu@51.222.244.45 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
