#!/bin/bash
#
# Usage: update.sh LANGUAGE

BASE_DIR=$(cd `dirname "$0"`; pwd)
TOONZLANG=$1

[ -d "${BASE_DIR}/${TOONZLANG}" ] || mkdir -p "${BASE_DIR}/${TOONZLANG}"
cd "${BASE_DIR}/${TOONZLANG}"
lupdate ../../colorfx/ -ts colorfx.ts
lupdate ../../common/ -ts tnzcore.ts
lupdate ../../tnztools/ -ts tnztools.ts
lupdate ../../toonz/ -ts toonz.ts
lupdate ../../toonzlib/ -ts toonzlib.ts
lupdate ../../toonzqt/ -ts toonzqt.ts
lupdate ../../image/ -ts image.ts

