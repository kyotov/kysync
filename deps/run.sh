export TYPE=Debug

mkdir $TYPE
cd $TYPE
cmake -DCMAKE_BUILD_TYPE=$TYPE --log-level=VERBOSE -G "Unix Makefiles" ..
cmake --build . --config $TYPE

unset TYPE