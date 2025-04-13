#!/usr/bin/env bash

mkdir -p build_macos/
cd build_macos

export DYLD_LIBRARY_PATH=/usr/local/lib
export DYLD_FALLBACK_LIBRARY_PATH=/usr/local/lib:
export VK_LAYER_PATH=/usr/local/opt/vulkan-profiles/share/vulkan/explicit_layer.d:/usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
export VK_ICD_FILENAMES=/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json

# export VK_LOADER_DEBUG=all  # Use this to debug configuration issues.

# \@bug:
#      When using macOS (Intel at least) validationlayers, from the macOS
#      distribution, the validation layer comes without a path, which breaks
#      loading it when creating the instance.
#
#  The solution seems to be running this sed command to hard-code the path:
#
#      sed -i '' 's#"library_path": "libVkLayer_khronos_validation.dylib"#"library_path": "/usr/local/lib/libVkLayer_khronos_validation.dylib"#' /usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json
#
#


cmake .. \
      -G Ninja \
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
      -D FLTK_BACKEND_WAYLAND=OFF \
      -D FLTK_BACKEND_X11=OFF \
      -D GLU_LIB="" \
      -D LIB_GL="" \
      -D LIB_MesaGL="" \
      -D OPENGL_INCLUDE_DIR="" \
      -D X11_xcb_xcb_INCLUDE_PATH="" 

#ninja && bin/test/vk_shape-shared
#ninja && bin/test/vk_shape_textured-shared
ninja && bin/test/vk_cube-shared
