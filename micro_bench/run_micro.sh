path_source=$(realpath -e main.cpp)
g++ $path_source -msse2 -o "${path_source%.cpp}.out" -Iinclude

source $CONFIG_PATH_CFG
ExecuteBench "micro_bench" "${path_source%.cpp}.out"
