#
#  build_macos.sh
#  applaudio
#
#  Created by Rasmus Anthin on 2025-09-18.
#

g++ test.cpp -o test -std=c++20 -I../include -I../../Core/include -framework AudioToolbox -framework CoreAudio -framework CoreFoundation
