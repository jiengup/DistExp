#!/bin/bash
set -e

if command -v yum &> /dev/null; then
    echo "Detected package manager: yum"
    sudo yum update
    sudo yum install -y cmake gperftools-devel glpk-devel
elif command -v apt-get &> /dev/null; then
    echo "Detected package manager: apt"
    sudo apt update
    sudo apt-get install -y cmake cmake google-perftools libgoogle-perftools-dev libglpk-dev
else
    echo "No supported package manager found"
    exit
fi

pushd .

cd ./thirdparty/
unzip gf-complete-master.zip
unzip Jerasure-master.zip
unzip spdlog-master.zip

cd gf-complete-master
./autogen.sh
./configure
make -j
sudo make install

cd ..
cd Jerasure-master
autoreconf --force --install
./configure
make -j
sudo make install

cd ..
cd spdlog-master
rm -rf build && mkdir build && cd build
cmake .. && make -j
sudo make install

popd

rm -rf build && mkdir build && cd build
cmake ..
make -j

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
