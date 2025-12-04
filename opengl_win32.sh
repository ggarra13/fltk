#!/usr/bin/env bash

rm -rf build_opengl_win32
mkdir -p build_opengl_win32/
cd build_opengl_win32

set +e

cmake .. \
      -G Ninja \
      -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_EXAMPLES=ON \
      -D FLTK_BUILD_FLUID=ON \
      -D FLTK_BUILD_GL=ON \
      -D FLTK_BUILD_SHARED_LIBS=ON \
      -D FLTK_BACKEND_WAYLAND=OFF \
      -D FLTK_BACKEND_X11=OFF

ninja -v

export PATH=$PWD/lib/:$PATH
bin/test/shape-shared
