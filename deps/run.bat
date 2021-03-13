call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set TYPE=Debug

mkdir %TYPE%
chdir %TYPE%
cmake -DCMAKE_BUILD_TYPE=%TYPE% --log-level=VERBOSE -G "NMake Makefiles" ..
cmake --build . --target %1 --config %TYPE%


Alternately, on Linux, you can do:
    export TYPE=Debug

    mkdir $TYPE
    cd $TYPE
    cmake -DCMAKE_BUILD_TYPE=$TYPE --log-level=VERBOSE -G "Unix Makefiles" ..
    cmake --build . --config $TYPE

    unset TYPE