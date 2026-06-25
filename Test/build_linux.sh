#
#  build_linux.sh
#  applaudio
#
#  Created by Rasmus Anthin on 2025-09-18.
#

export BUILD_PKG_CONFIG_MODULES=alsa
g++ test.cpp -o test -std=c++20 -I../include $(pkg-config --cflags --libs $BUILD_PKG_CONFIG_MODULES)
