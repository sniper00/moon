#!/bin/bash

git submodule init
git submodule update
git pull

# Detect the operating system
os=$(uname)

if [ "$os" == "Linux" ]; then
    # Execute Linux commands
    echo "Running on Linux"
    cpu_num=$(cat /proc/cpuinfo | grep "processor" | wc -l)
    make config=release -j$cpu_num
elif [ "$os" == "Darwin" ]; then
    # Execute macOS commands
    # /bin/bash -c "$(curl -fsSL https://gitee.com/ineo6/homebrew-install/raw/master/install.sh)"
    echo "Running on macOS"
    cpu_num=$(sysctl -n machdep.cpu.thread_count)
    brew install premake
    premake5 gmake --cc=clang
    make config=release
else
  echo "Unknown operating system"
  exit 1
fi