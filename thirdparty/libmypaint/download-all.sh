#!/bin/bash

set -e

echo ""
echo "download"
echo ""

mkdir -p download && cd download

wget -c https://ftp.gnu.org/pub/gnu/gettext/gettext-0.19.7.tar.gz
wget -c https://s3.amazonaws.com/json-c_releases/releases/json-c-0.12.1.tar.gz
wget -c https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz

cd ..

echo ""
echo "unpack"
echo ""

mkdir -p src && cd src

rm  -rf              gettext-0.19.7
tar -xzf ../download/gettext-0.19.7.tar.gz
rm  -rf              json-c-0.12.1
tar -xzf ../download/json-c-0.12.1.tar.gz
rm  -rf              libiconv-1.15
tar -xzf ../download/libiconv-1.15.tar.gz

echo ""
echo "checkout libmypaint"
echo ""

BRANCH="testing"
if [ -d "libmypaint/.git" ]; then
    cd libmypaint && git fetch && git reset --hard "origin/$BRANCH" && cd ..
else
    git clone https://github.com/blackwarthog/libmypaint.git --branch $BRANCH
fi

cd ..

echo ""
echo "success"
echo ""
