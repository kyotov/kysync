#!/bin/bash -ox

export TYPE=Debug

mkdir $TYPE

cd $TYPE || exit

cmake -DCMAKE_BUILD_TYPE=$TYPE --log-level=VERBOSE -G "Unix Makefiles" ..
cmake --build . --target "$1" --config $TYPE

unset TYPE