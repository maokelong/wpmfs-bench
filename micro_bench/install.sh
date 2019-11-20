CONFIG_CMD_MICRO=""

path_source=$(realpath -e main.cpp)
g++ $path_source -msse2 -o "${path_source%.cpp}.out" -Iinclude
CONFIG_CMD_MICRO="${path_source%.cpp}.out\n"
