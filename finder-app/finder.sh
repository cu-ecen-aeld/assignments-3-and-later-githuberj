#!/bin/bash

# Check if the arguments are specified
if [ $# -ne 2 ]; then
    echo "Error: Please specify the path to the directory and the text string to be searched within the files."
    exit 1
fi

# Check if the first argument is a directory
if [ ! -d "$1" ]; then
    echo "Error: The first argument must be a path to a directory on the filesystem."
    exit 1
fi

# Search for the text string in the files and count the number of matches
num_files=$(find "$1" -type f | wc -l)
num_matches=$(grep -r "$2" "$1" | wc -l)

# Print the number of files and matching lines
echo "The number of files are $num_files and the number of matching lines are $num_matches."
