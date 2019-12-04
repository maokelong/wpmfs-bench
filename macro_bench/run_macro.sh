#!/bin/bash
set -e

#===========================================
# Zone Functions
#===========================================

RunFileBench() {
    source $CONFIG_PATH_CFG
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

RunWhisperCMD() {
    source $CONFIG_PATH_CFG
    cmd=$@
    ExecuteBench $(basename ${cmd%% *}) $cmd
}

RunWhisper() {
    if [[ ! -f tmpflag ]]; then
        find ./ -type f -readable -writable -exec sed -i -e "s@/mnt/wpmfs@/mnt/wpmfs@g" {} +
        find ./ -type f -name "*.h" -readable -writable -exec sed -i -e "s/1UL \* 1024 \* 1024 \* 1024/4UL * 1024 * 1024 * 1024/g" {} +
        find ./ -type f -name "*.sh" -readable -writable -exec sed -i -E "s@bin=(\..+)@bin=\"RunWhisperCMD \1\"@g" {} +
        find ./ -type f -name "*.sh" -readable -writable -exec sed -i -E "s@bin=(\"\..+)@bin=RunWhisperCMD\\\ \1@g" {} +
        touch tmpflag
    fi

    #TODO: to support more benchmarks from whisper
    # list_whisper="ycsb tpcc echo"
    list_whisper="tpcc"
    pushd whisper
    for whisper_bench in $list_whisper; do
        ./script.py -r -z large -w $whisper_bench
    done
    popd
}

#===========================================
# Main
#===========================================

if [[ $# -ne 0 ]]; then
    RunWhisperCMD $@
    exit 0
fi

RunFileBench
RunWhisper
