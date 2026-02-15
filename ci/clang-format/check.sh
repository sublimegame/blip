#!/bin/sh

# 
# considered paths
# 
# ./ci/C
# ./core

# dry-run (check if format is already ok)
docker run --rm -w /workdir -v $(pwd):/workdir gaetan/clang-tools sh -c "find ./ci/C -regex '.*\.\(cpp\|hpp\|c\|h\)' -print0 | xargs -0 clang-format --dry-run --Werror -style=file"
docker run --rm -w /workdir -v $(pwd):/workdir gaetan/clang-tools sh -c "find ./core -regex '.*\.\(cpp\|hpp\|c\|h\)' -print0 | xargs -0 clang-format --dry-run --Werror -style=file"

# =================================================

# using an entrypoint in the container
# docker run -v $(pwd):/workdir gaetan/clang-tools format/tidy <path> '<regex>'
# 
# Args
# ----------
# 1st : `format` or `tidy` indicating whether clang-format or clang-tidy is to be used
# 2nd : path at which to run the chosen tool
# 3rd : regex to use to select the files to process
# 4th : `dry-run` or `modify`

# docker run --rm -v $(pwd):/workdir gaetan/clang-tools format ./ci/C '.*\.\(cpp\|hpp\|c\|h\)' dry-run
# docker run --rm -v $(pwd):/workdir gaetan/clang-tools format ./ci/C '.*\.\(cpp\|hpp\|c\|h\)' modify
