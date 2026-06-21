#!/usr/bin/env bash

#rm -rf build_gl_x11
mkdir -p build_gl_macos/
cd build_gl_macos

set +e

cmake .. \
      -G Ninja \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_EXAMPLES=ON \
      -D FLTK_BUILD_FLUID=ON \
      -D FLTK_BUILD_GL=ON \
      -D FLTK_BACKEND_WAYLAND=OFF \
      -D FLTK_BACKEND_X11=OFF

ninja -v

bin/test/shape
