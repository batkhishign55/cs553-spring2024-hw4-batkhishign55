#!/bin/bash

echo "Running tests..."

# hash threads
for i in 1 4 16
do
    # sort threads
    for j in 1 4 16
    do
        # write threads
        for k in 1 4 16
        do
            ./hashgen -t $i -o $j -i $k -s 1024 >> ./result.log
            echo -e "\tRan with hash thread: $i, sort thread: $j, write thread: $k."
            sleep 1
        done
    done
done