sudo add-apt-repository --yes ppa:beineri/opt-qt591-trusty
sudo add-apt-repository --yes ppa:achadwick/mypaint-testing
sudo apt-get update
sudo apt-get install -y liblzo2-dev liblz4-dev libfreetype6-dev libpng-dev libegl1-mesa-dev libgles2-mesa-dev libsdl2-dev libglew-dev freeglut3-dev qt59script libsuperlu3-dev qt59svg qt59tools qt59multimedia wget libusb-1.0-0-dev libboost-all-dev liblzma-dev libjson-c-dev libmypaint-dev

# someone forgot to include liblz4.pc with the package, use the version from xenial, as it only depends on libc
wget http://mirrors.kernel.org/ubuntu/pool/main/l/lz4/liblz4-1_0.0~r131-2ubuntu2_amd64.deb -O liblz4.deb
wget http://mirrors.kernel.org/ubuntu/pool/main/l/lz4/liblz4-dev_0.0~r131-2ubuntu2_amd64.deb -O liblz4-dev.deb
sudo dpkg -i liblz4.deb liblz4-dev.deb
