#!/bin/bash
set -e

#===========================================
#	Zone Configs
#===========================================

CONFIG_SRC_PIN="https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz"
CONFIG_SRC_WPMFS="https://github.com/maokelong/wpmfs"

export CONFIG_PATH_ROOT=$(pwd)
CONFIG_PATH_TOOLS="$CONFIG_PATH_ROOT/tools"
export CONFIG_PATH_OUTPUT="$CONFIG_PATH_ROOT/output"
CONFIG_PATH_PINTOOL="$CONFIG_PATH_TOOLS/wpmfs-pintool.cpp"
export CONFIG_PATH_PY_DUMP_WR_DIS="$CONFIG_PATH_TOOLS/dump_write_distribution.py"

pin_dir=$(basename $CONFIG_SRC_PIN .tar.gz)
export CONFIG_PIN_PATH="$CONFIG_PATH_TOOLS/$pin_dir"
export CONFIG_PIN_SO_PATH

#===========================================
#	Zone Functions
#===========================================

echog() {
    echo -e "\e[32m"$@"\e[39m"
}
export -f echog

echor() {
    echo -e "\e[31m"$@"\e[39m"
}
export -f echor

InstallWpmfs() {
    pushd $CONFIG_PATH_ROOT/wpmfs
    bash scripts/install.sh
    popd
    echog "Wpmfs-bench: Wpmfs (re)installed."
}
export -f InstallWpmfs

# format: ExecuteBench bench_name cmd
ExecuteBench() {
    if [[ $# -le 1 ]]; then
        echor "Wpmfs-bench: No bench to run."
    fi
    
    mkdir -p $CONFIG_PATH_OUTPUT
    args=$*
    bench=$(basename ${args%% *})
    cmd=${args:${#1}}
    pin_output="${CONFIG_PATH_OUTPUT}/raw.$bench"
    wrdis_graph="${CONFIG_PATH_OUTPUT}/graph.$bench.png"
    
    InstallWpmfs
    sudo time $CONFIG_PIN_PATH/pin \
    -logfile "${CONFIG_PATH_OUTPUT}/log.$bench" \
    -t $CONFIG_PIN_SO_PATH \
    -o $pin_output \
    -- $cmd
    
    python $CONFIG_PATH_PY_DUMP_WR_DIS $pin_output $wrdis_graph
    echog "Wpmfs-bench: cmd $cmd finished."
}
export -f ExecuteBench

#===========================================
#	Main
#===========================================

# 如果没有解压 pin，就解压 pin
pin_tar=$(basename $CONFIG_SRC_PIN)
if [[ ! -d $CONFIG_PIN_PATH ]]; then
    pushd $CONFIG_PATH_TOOLS
    # 如果没有下载 pin，就下载 pin
    if [[ ! -f $pin_tar ]]; then
        wget $CONFIG_SRC_PIN
    fi
    tar -xvzf $pin_tar
    # rm -rf $pin_tar
    popd
fi

echog "Wpmfs-bench: Pin installed"

# 拷贝 pintool 到 pin 的特定目录并 make
pin_so=$(basename $CONFIG_PATH_PINTOOL .cpp).so
cp $CONFIG_PATH_PINTOOL $CONFIG_PIN_PATH/source/tools/MyPinTool
pushd $CONFIG_PIN_PATH/source/tools/MyPinTool
mkdir -p obj-intel64
make obj-intel64/$pin_so
CONFIG_PIN_SO_PATH=$(realpath -e obj-intel64/$pin_so)
popd
echog "Wpmfs-bench: Pintool installed."

# clear syslog
sudo bash -c 'echo "" > /var/log/syslog'
echog "Wpmfs-bench: /var/log/syslog cleared."

# mount/remount wpmfs
if [[ ! -d wpmfs ]]; then
    git clone $CONFIG_SRC_WPMFS wpmfs
fi

pushd micro_bench
bash run_micro.sh
popd

pushd macro_bench
bash run_macro.sh
popd
