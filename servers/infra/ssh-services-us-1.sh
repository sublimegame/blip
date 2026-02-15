#!/bin/bash

#
# services-us-1
#

set -e

# login with SSH key
ssh ubuntu@15.235.116.53 -i ~/.ssh/id_ovh_voxowl_com -o IdentitiesOnly=yes
