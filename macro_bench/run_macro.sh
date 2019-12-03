#!/bin/bash
set -e

RunFileBench() {
    path_filebenches="$(pwd)/filebench_selected"
    list_filebenches="fileserver.f webproxy.f webserver.f"
    
    # install File Bench
    if [[ ! -d $path_filebenches ]]; then
        mkdir -p $path_filebenches
        for file_bench in $list_filebenches; do
            cp filebench/workloads/$file_bench $path_filebenches
            sed -i 's@set $dir=/tmp@set $dir=/mnt/wpmfs@g' $path_filebenches/$file_bench
            CONFIG_CMD_MACRO="\n$CONFIG_CMD_MACRO"
        done
    fi
    
    for file_bench in $list_filebenches; do
        bench_code=$(basename ${file_bench})
        ExecuteBench $bench_code "filebench -f $path_filebenches/$file_bench"
    done
}
    
RunWhisper() {
    #TODO: finish me later
    find ./ -type f -readable -writable -exec sed -i -e "s/\/dev\/shm/\/mnt\/wpmfs/g" {} +
    find ./ -type f -name "*.h" -readable -writable -exec sed -i -e "s/1UL \* 1024 \* 1024 \* 1024/4UL * 1024 * 1024 * 1024/g" {} +
    find ./ -type f -name "*.sh" -readable -writable -exec sed -i -E "s@bin=(\..+)@bin=\"$(realpath $0) \1\"@g" {} +
    find ./ -type f -name "*.sh" -readable -writable -exec sed -i -E "s@bin=(\"\..+)@bin=$(realpath $0) \1@g" {} +
    
    list_whisper="ycsb tpcc echo"
    pushd whisper
    for whisper_bench in $list_whisper; do
        ./script.py -r -z small -w $whisper_bench
    done
    popd
}

RunWhisperCMD() {
    #TODO: finish me later
    echo $*
}

if [[ $# -ne 0 ]]; then
    ReadWhisperCMD $*
    echo $*
    exit 0
fi

RunFileBench
