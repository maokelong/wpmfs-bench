#!/bin/bash
set -e
source configs.sh

# download pin
echo "$CONFIG_SRC_PIN"

pin_tar=$(basename $CONFIG_SRC_PIN)
if [[ ! -d $CONFIG_PIN_PATH ]]; then
    pushd $CONFIG_PATH_TOOLS
    if [[ ! -f $pin_tar ]]; then
        wget $CONFIG_SRC_PIN
    fi
    tar -xvzf $pin_tar
    # rm -rf $pin_tar
    popd
fi

echog "Wpmfs-bench: Pin(The platform) installed"

# install our customized pintool
pin_so=$(basename $CONFIG_PATH_PINTOOL .cpp).so
cp $CONFIG_PATH_PINTOOL $CONFIG_PIN_PATH/source/tools/MyPinTool
pushd $CONFIG_PIN_PATH/source/tools/MyPinTool
mkdir -p obj-intel64
make obj-intel64/$pin_so
CONFIG_PIN_SO_PATH=$(realpath -e obj-intel64/$pin_so)
popd
echog "Wpmfs-bench: Pintool(The dynamic library) installed."

# clear syslog
sudo bash -c 'echo "" > /var/log/syslog'
echog "Wpmfs-bench: /var/log/syslog cleared."

# download wpmfs
if [[ ! -d wpmfs || "`ls wpmfs`" = "" ]]; then
    git clone $CONFIG_SRC_WPMFS wpmfs
    echog "Wpmfs-bench: Wpmfs installed."
fi
InstallWpmfs

pushd micro_bench
# bash run_micro.sh
popd

pushd macro_bench
chmod +x run_macro.sh
bash run_macro.sh
popd
