#!/bin/bash

#
# gameservers-sg-1
#

set -e

# login with SSH key
ssh ubuntu@15.235.181.168 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
