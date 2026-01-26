#!/bin/bash

# 0 argument
if [ $# -eq 0 ] 
then
    set -e

    echo "----------------- make deps -----------------"
    make deps

    echo "----------------- make ci -------------------"
    make ci

    echo "----------------- run ci --------------------"
    ./ci
fi



# 1 argument (output path)
# - exit status is always 0 (success)
# - "success" or "failure" is written in the file at the provided path
if [ $# -eq 1 ] 
then
    echo "----------------- make deps -----------------"
    make deps || {
        echo "failure" > $1
        exit 0
    }

    echo "----------------- make ci -------------------"
    make ci || {
        echo "failure" > $1
        exit 0
    }

    echo "----------------- run ci --------------------"
    ./ci || {
        echo "failure" > $1
        exit 0
    }

    echo "success" > $1
fi
