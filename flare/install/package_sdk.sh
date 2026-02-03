#!/bin/bash

while getopts t:d:h OPT
do
    case $OPT in
        t) TOONZREPO=$OPTARG
           ;;
        d) DIST=$OPTARG
           ;;
        h) echo "-t opentoonz_repository_dir -d dist_dir"
           exit 1;
           ;;
        \?) exit 1
            ;;
    esac
done

# Allow an alternate env var for the new repo name (FLAREREPO) as a fallback
if [ -z "$TOONZREPO" ]; then
  if [ -n "$FLAREREPO" ]; then
    TOONZREPO=$FLAREREPO
  else
    echo "Repository path not specified. Use -t <repo_dir> or set FLAREREPO env var."
    exit 1
  fi
fi

mkdir -p $DIST
pushd $DIST

mkdir core
mkdir utils
mkdir doc

cp $TOONZREPO/LICENSE.txt ./
# Source code has been moved into the 'flare' top-level folder; keep the original internal lib names
cp $TOONZREPO/flare/sources/toonzqt/toonz_plugin.h core/
cp $TOONZREPO/flare/sources/toonzqt/toonz_hostif.h core/
cp $TOONZREPO/flare/sources/toonzqt/toonz_params.h core/

cp $TOONZREPO/plugins/utils/affine.hpp utils/
cp $TOONZREPO/plugins/utils/rect.hpp utils/
cp $TOONZREPO/plugins/utils/interf_holder.hpp utils/
cp $TOONZREPO/plugins/utils/param_traits.hpp utils/

# copy samples

mkdir -p samples/blur
mkdir -p samples/geom
mkdir -p samples/multiplugin

cp $TOONZREPO/plugins/blur/* samples/blur/
cp $TOONZREPO/plugins/geom/* samples/geom/
cp $TOONZREPO/plugins/multiplugin/* samples/multiplugin/

find ./samples -iname CMakeLists.txt -exec sed -e 's/set(PLUGINSDK_ROOT.*)/set(PLUGINSDK_ROOT \.\.\/\.\.\/core)/' -i "" {} \;
find ./samples -iname CMakeLists.txt -exec sed -e 's/set(PLUGINSDK_UTILS_PATH.*)/set(PLUGINSDK_UTILS_PATH \.\.\/\.\.\/)/' -i "" {} \;

popd
