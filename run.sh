#!/bin/bash
set -e

CONFIG_SRC_PIN="https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.7-97619-g0d0c92f4f-gcc-linux.tar.gz"
CONFIG_SRC_WPMFS="https://github.com/maokelong/wpmfs"

CONFIG_PATH_TOOLS="$(pwd)/tools"
CONFIG_PATH_OUTPUT="$(pwd)/output"

CONFIG_PATH_PINTOOL="$CONFIG_PATH_TOOLS/wpmfs-pintool.cpp"
CONFIG_PATH_PY_DUMP_WR_DIS="$CONFIG_PATH_TOOLS/dump_write_distribution.py"

# 如果没有解压 pin，就解压 pin
pin_tar=$(basename $CONFIG_SRC_PIN)
pin_dir=$(basename $CONFIG_SRC_PIN .tar.gz)
pin_path="$CONFIG_PATH_TOOLS/$pin_dir"
if [[ ! -d $pin_path ]]; then
  pushd $CONFIG_PATH_TOOLS
  # 如果没有下载 pin，就下载 pin
  if [[ ! -f $pin_tar ]]; then
    wget $CONFIG_SRC_PIN
  fi
  tar -xvzf $pin_tar
  # rm -rf $pin_tar
  popd
fi
echo -e "\e[32mWpmfs-bench: Pin installed.\e[39m"

# 拷贝 pintool 到 pin 的特定目录并 make
pin_so=$(basename $CONFIG_PATH_PINTOOL .cpp).so
cp $CONFIG_PATH_PINTOOL $pin_path/source/tools/MyPinTool
pushd $pin_path/source/tools/MyPinTool
mkdir -p obj-intel64
make obj-intel64/$pin_so
pin_so_path=$(realpath -e obj-intel64/$pin_so)
popd
echo -e "\e[32mWpmfs-bench: Pintool installed.\e[39m"

# mount/remount wpmfs
if [[ ! -d wpmfs ]]; then
  git clone $CONFIG_SRC_WPMFS wpmfs
fi
pushd wpmfs
bash scripts/install.sh
popd
echo -e "\e[32mWpmfs-bench: Wpmfs installed.\e[39m"

# install micro benchmark
pushd micro_bench
source install.sh
popd
echo -e "\e[32mWpmfs-bench: Micro-benchmark installed.\e[39m"

mkdir -p $CONFIG_PATH_OUTPUT

# run benchmarks
time $pin_path/pin \
  -logfile "${CONFIG_PATH_OUTPUT}/pin.log" \
  -t $pin_so_path \
  -o "${CONFIG_PATH_OUTPUT}/pin.output" \
  -- "${CONFIG_PATH_MICRO%.cpp}.out"
echo -e "\e[32mWpmfs-bench: Micro-benchmark finished.\e[39m"

wr_dis_graph="${CONFIG_PATH_OUTPUT}/wr_dis.png"
python $CONFIG_PATH_PY_DUMP_WR_DIS \
  "${CONFIG_PATH_OUTPUT}/pin.output" \
  $wr_dis_graph
echo -e "\e[32mWpmfs-bench: Write distribution of wpmfs is saved in $wr_dis_graph.\e[39m"

# TODO: 安装配置 whipser
# TODO: 安装配置 filebench
# sudo time $pin_path/pin \
#   -logfile "${CONFIG_PATH_OUTPUT}/pin.log" \
#   -t $pin_so_path  \
#   -o "${CONFIG_PATH_OUTPUT}/pin.output" \
#   -- filebench -f macro_bench/webserver.f
