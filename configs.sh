#!/bin/bash
set -e

#===========================================
# Zone Configs
#===========================================

CONFIG_SRC_PIN="https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz"
CONFIG_SRC_WPMFS="https://github.com/maokelong/wpmfs"

if [[ ! $CONFIG_PATH_CFG ]]; then
    # fst sourced by wpmfs-bench/run.sh
    export CONFIG_PATH_CFG=$(dirname $(realpath $0))/configs.sh
fi

CONFIG_PATH_ROOT=$(dirname $CONFIG_PATH_CFG)
CONFIG_PATH_TOOLS="$CONFIG_PATH_ROOT/tools"
CONFIG_PATH_OUTPUT="$CONFIG_PATH_ROOT/output"
CONFIG_PATH_PINTOOL="$CONFIG_PATH_TOOLS/wpmfs-pintool.cpp"
CONFIG_PATH_PY_DUMP_WR_DIS="$CONFIG_PATH_TOOLS/dump_write_distribution.py"
CONFIG_PATH_PY_DUMP_EXP_LIFETIME="$CONFIG_PATH_TOOLS/dump_expected_lifetime.py"

pin_dir=$(basename $CONFIG_SRC_PIN .tar.gz)
CONFIG_PIN_PATH="$CONFIG_PATH_TOOLS/$pin_dir"

# update in wpmfs-bench/run.sh
export CONFIG_PIN_SO_PATH

#===========================================
#	Zone Functions
#===========================================

echog() {
    # g = green
    echo -e "\e[32m"$@"\e[39m"
}

echor() {
    # r = red
    echo -e "\e[31m"$@"\e[39m"
}

InstallWpmfs() {
    pushd $CONFIG_PATH_ROOT/wpmfs
    bash scripts/install.sh
    popd
    echog "Wpmfs-bench: Wpmfs (re)installed."
}

# format: ExecuteBench bench_name cmd
ExecuteBench() {
    if [[ $# -le 1 ]]; then
        echor "Wpmfs-bench: No bench to run."
    fi
    
    mkdir -p $CONFIG_PATH_OUTPUT
    args=$@
    bench=$(basename ${args%% *})
    cmd=${args:${#1}}
    pin_output="${CONFIG_PATH_OUTPUT}/raw.$bench"
    wrdis_graph="${CONFIG_PATH_OUTPUT}/graph.wrdis.$bench"
    explife_graph="${CONFIG_PATH_OUTPUT}/graph.explife.$bench"
    
    # to addionally support whisper-echo
    if [[ $bench == "evaluation" ]]; then
        sudo rm -f /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libkp_kvstore.so.1
        sudo ln -s $(realpath ./malloc/lib/libtcmalloc_minimal.so.4.1.0) /usr/lib/libtcmalloc_minimal.so.4
        sudo ln -s $(realpath ../lib/libkp_kvstore.so.1.0.1) /usr/lib/libkp_kvstore.so.1
    fi
    
    InstallWpmfs
    sudo time $CONFIG_PIN_PATH/pin \
    -logfile "${CONFIG_PATH_OUTPUT}/log.$bench" \
    -t $CONFIG_PIN_SO_PATH \
    -o $pin_output \
    -- $cmd
    
    python $CONFIG_PATH_PY_DUMP_WR_DIS $pin_output $wrdis_graph
    python $CONFIG_PATH_PY_DUMP_EXP_LIFETIME $pin_output $explife_graph 60 20
    echog "Wpmfs-bench: cmd $cmd finished."
}
