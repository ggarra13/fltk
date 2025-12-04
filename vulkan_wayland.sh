#!/usr/bin/env bash

mkdir -p build_wayland/
cd build_wayland

set +e

export VULKAN_SDK=/usr/

#
# Whether to use explicit sync or implicit sync.
#
# GNOME48 and Kwin6 on Ubuntu 25.04 default is implicit sync.
# GNOME49 and Kwin6 on Ubuntu 25.10 default is explicit sync.
#
# Sets the variable correctly for each graphics card (currently only works
# in NVidia, which seems to have a timeout when using HDR it with events on
# both GNOME49 and Kwin6.2).
#
if [[ -z $FLTK_VK_EXPLICIT_SYNC ]]; then
    export FLTK_VK_EXPLICIT_SYNC=0
fi

cmake .. \
      -G Ninja \
      -D CMAKE_CXX_STANDARD=17 \
      -D CMAKE_BUILD_TYPE=Debug \
      -D CMAKE_INSTALL_PREFIX=$PWD/install \
      -D FLTK_BUILD_EXAMPLES=OFF \
      -D FLTK_BUILD_FLUID=OFF \
      -D FLTK_BUILD_FORMS=OFF \
      -D FLTK_BUILD_GL=OFF \
      -D FLTK_BUILD_VK=ON \
      -D FLTK_BUILD_PDF_DOCS=OFF \
      -D FLTK_BUILD_HTML_DOCS=OFF \
      -D FLTK_BUILD_OPTIONS=OFF \
      -D FLTK_BUILD_SHARED_LIBS=ON \
      -D FLTK_BUILD_TEST=ON \
      -D FLTK_GRAPHICS_CAIRO=OFF \
      -D FLTK_OPTION_CAIRO_EXT=OFF \
      -D FLTK_OPTION_CAIRO_WINDOW=OFF \
      -D FLTK_OPTION_PRINT_SUPPORT=OFF \
      -D FLTK_OPTION_STD=ON \
      -D FLTK_OPTION_SVG=OFF \
      -D FLTK_USE_PANGO=OFF \
      -D FLTK_USE_PTHREADS=OFF \
      -D FLTK_USE_SYSTEM_LIBJPEG=OFF \
      -D FLTK_USE_SYSTEM_LIBPNG=OFF \
      -D FLTK_USE_SYSTEM_ZLIB=OFF \
      -D FLTK_USE_SYSTEM_LIBDECOR=OFF \
      -D FLTK_USE_XCURSOR=OFF \
      -D FLTK_USE_XFIXES=OFF \
      -D FLTK_USE_XFT=OFF \
      -D FLTK_USE_XINERAMA=OFF \
      -D FLTK_USE_XRENDER=OFF \
      -D FLTK_USE_LIBDECOR_GTK=OFF \
      -D FLTK_BACKEND_WAYLAND=ON \
      -D FLTK_BACKEND_X11=OFF

ninja -v

#bin/test/vk_shape-shared
#bin/test/vk_shape_textured-shared
#bin/test/vk_cube-shared
bin/test/vk_fullscreen-shared
