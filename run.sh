#!/bin/bash
set -e

CONFIG_SRC_PIN="https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz"
CONFIG_SRC_WPMFS="https://github.com/maokelong/wpmfs"

CONFIG_PATH_PINTOOL="wpmfs-pintool.cpp"
CONFIG_PATH_OUTPUT="output"
CONFIG_PATH_DBG="dbg.cpp" # cpp, sse2

# 如果没有解压 pin，就解压 pin
if [[ ! -d $(basename $CONFIG_SRC_PIN .tar.gz) ]]; then
  # 如果没有下载 pin，就下载 pin
  if [[ ! -f $(basename $CONFIG_SRC_PIN) ]]; then
    wget $CONFIG_SRC_PIN
  fi
  tar -xvzf $(basename $CONFIG_SRC_PIN)
  rm -rf $(basename $CONFIG_SRC_PIN)
fi

# 拷贝 pintool 到 pin 的特定目录并 make
cp $CONFIG_PATH_PINTOOL $(basename $CONFIG_SRC_PIN .tar.gz)/source/tools/MyPinTool
pushd $(basename $CONFIG_SRC_PIN .tar.gz)/source/tools/MyPinTool
mkdir -p obj-intel64
make obj-intel64/$(basename $CONFIG_PATH_PINTOOL .cpp).so
popd

# mount/remount wpmfs
if [[ ! -d wpmfs ]]; then
  git clone https://github.com/maokelong/wpmfs wpmfs
fi
pushd wpmfs
bash scripts/install.sh
popd

# TODO-MKL: 安装配置 whipser

# 生成测试用应用
g++ $CONFIG_PATH_DBG -msse2 -o "${CONFIG_PATH_DBG%.cpp}.out"

# 使用生成的 pintool 插桩应用
if [[ ! -d $CONFIG_PATH_OUTPUT ]]; then
  mkdir -p $CONFIG_PATH_OUTPUT
fi
$(basename $CONFIG_SRC_PIN .tar.gz)/pin \
  -logfile "${CONFIG_PATH_OUTPUT}/pin.log" \
  -t $(basename $CONFIG_SRC_PIN .tar.gz)/source/tools/MyPinTool/obj-intel64/$(basename $CONFIG_PATH_PINTOOL .cpp).so \
  -o "${CONFIG_PATH_OUTPUT}/pin.output" \
  -- ./"${CONFIG_PATH_DBG%.cpp}.out"
