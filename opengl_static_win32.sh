#!/usr/bin/env bash

rm -rf build_static_opengl_x11
mkdir -p build_static_opengl_win32/
cd build_static_opengl_win32

set +e

cmake .. \
      -G Ninja \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_SHARED_LIBS=OFF \
      -D FLTK_BUILD_EXAMPLES=ON \
      -D FLTK_BUILD_FLUID=ON \
      -D FLTK_BUILD_GL=ON

ninja -v

bin/test/gl_overlay
