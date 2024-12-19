#!/bin/bash

set -e

dir=$(dirname "$0")
cp $dir/../../corrade/modules/FindCorrade.cmake $dir/../modules/
cp $dir/../../magnum/modules/FindMagnum.cmake $dir/../modules/
