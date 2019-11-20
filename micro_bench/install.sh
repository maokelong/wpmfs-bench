CONFIG_PATH_MICRO=$(realpath -e main.cpp)
g++ $CONFIG_PATH_MICRO -msse2 -o "${CONFIG_PATH_MICRO%.cpp}.out" -Iinclude
