#!/bin/bash

set -e

for i in *.in; do
    ./in2ply.py ${i}
done
