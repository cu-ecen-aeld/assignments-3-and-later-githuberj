#!/bin/bash

# Check if two args are specified
if [ $# -ne 2 ]; then
    echo "Error: Please specify the full path to the file and the text string to be written within the file."
    exit 1
fi
# create folder 
mkdir -p "$(dirname "$1")"
# write file
echo "$2" > "$1"

# Check if the file was created successfully
if [ $? -ne 0 ]; then
    echo "Error: Could not create the file."
    exit 1
fi
