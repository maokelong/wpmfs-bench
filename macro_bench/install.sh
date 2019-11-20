#!/bin/bash
set -e

CONFIG_CMD_MACRO=""
path_filebenches="$(pwd)/filebench_selected"
list_filebenches="fileserver.f webproxy.f webserver.f"

mkdir -p $path_filebenches
for file_bench in $list_filebenches; do
  cp filebench/workloads/$file_bench $path_filebenches
  sed -i 's/set $dir=\/tmp/set $dir=\/mnt\/wpmfs/g' $path_filebenches/$file_bench
  CONFIG_CMD_MACRO="filebench -f $path_filebenches/$file_bench\n$CONFIG_CMD_MACRO"
done
