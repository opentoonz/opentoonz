#!/bin/tcsh
cp $SCRIPTROOT/toonzdef.$1  $HOME/toonzdef  
source $HOME/.cshrc
source $HOME/.login
./copy_plugin.sh
./installName.sh

