#!/usr/bin/env bash

#rm -rf build_gl_x11
mkdir -p build_gl_x11/
cd build_gl_x11

set +e

if [[ -z "$VULKAN_SDK" ]]; then
    export VULKAN_SDK=/usr/
fi

cmake .. \
      -G Ninja \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_EXAMPLES=ON \
      -D FLTK_BUILD_GL=ON \
      -D FLTK_BACKEND_WAYLAND=OFF \
      -D FLTK_BACKEND_X11=ON

ninja

bin/test/gl_overlay
