README.Wayland.txt - Wayland Platform Support for FLTK
------------------------------------------------------


Contents
========

 1   Introduction

 2   Vulkan Support for FLTK
   2.1    Enabling Vulkan
   2.2    Configuration
   2.3    Known Limitations

 3   Platform Specific Notes
   3.1    Wayland
   3.2    X11
   3.3    macOS
   3.4    Windows

4   API

5   Demos

6   Projects

1 Introduction
==============

Version 1.5 of the FLTK library introduces support of drivers for the FLTK API
for Vulkan development. It requires a Vulkan-equipped OS, namely Win64 with new drivers, macOS with MoltenVK, or Linux with Khrono's Vulkan.
The code has currently been tested on Windows, X11, Wayland and macOS.

2 Vulkan Support for FLTK
=========================


 2.1 Enabling Vulkan
---------------------


Note: When building a user project with CMake 3.4 or higher, i.e. using

  cmake_minimum_required (VERSION 3.4)

or any higher (minimum) CMake version users need to use at least one of
the following techniques:

-D FLTK_USE_VK=ON
-D CMAKE_CXX_STANDARD=17   # or:
   			   # provide custom implementation of macro VMA_SYSTEM_ALIGNED_MALLOC (and VMA_SYSTEM_ALIGNED_FREE)


 2.2 Configuration
------------------

On Linux systems equipped with the adequate software packages
(see section 3 below)

 2.3 Known Limitations
----------------------

* Vulkan does not support adding FLTK widgets under the Vulkan scene like OpenGL does.

* Vulkan does not support drawing with the fl_draw/gl_draw functions.

* Currently, support for Vulkan's text rendering is not implemented.


3 Platform Specific Notes
=========================

The following are notes about building FLTK for the Vulkan platform
on the various supported Linux distributions/OS.

3.1 Debian and Derivatives (like Ubuntu, Mint, RaspberryPiOS)
-------------------------------------------------------------

sudo apt install libvulkan-dev glslang-dev libshaderc-dev spirv-tools

3.2 Fedora / Rocky Linux
------------------------

sudo dnf install vulkan-headers vulkan-loader-devel
sudo dnf install vulkan-tools vulkan-validation-layers-devel
sudo dnf install spirv-tools

# This may not be found and may require compiling from source
sudo dnf install shaderc

# Compiling from source
git clone https://github.com/google/shaderc.git
cd shaderc
git submodule update --init
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install

# Reboot


3.3 macOS
---------

Currently, Vulkan can be used by installing it from brew, compiling from source or downloading the latest version from LunarG.

brew install vulkan-loader
brew install molten-vk
brew install vulkan-tools
brew install shaderc
brew install glslang
brew install vulkan-profiles
brew install spirv-tools

Optional for developing:

brew install vulkan-validationslayers

Depending if you are on MacOS Intel or MacOS M1 and later, the location of:

environment variable VULKAN_SDK should be set to:

/usr/local    on Intel
/opt/homebrew on M1+

If you installed the version from LunarG, by default it will get installed in your home directory, like:

/home/<username>/VulkanSDK/<SDK version>/macOS

3.4 Windows
-----------

You need to download the Vulkan SDK from LunarG.   Once installed, the normal location will be C:\VulkanSDK.  You should set the environment variable VULKAN_SDK to that directory.

4.0 API

The API of Vulkan relies on two new classes: Fl_Vk_Window (which you must derive from) and Fl_Vk_Window_Driver (which you should not change).
All Vulkan primitives are kept in FL/Fl_Vk_Context.H and can be accessed by Fl_Vk_Windows' accessors.  This allows us to change the implementation of them as the API evolves, without breaking backwards compatibility.
Also, passing a reference or pointer of Fl_Vk_Context is simpler and more efficient than passing each Vulkan handle individually.

These are responsible for setting up a Vulkan Window.  There's no Fl_Vk_Instance like other APIs like Qt.  Instead a number of functions should be overriden and return std::vectors.
All extensions (VkInstance and VkDevice ones) are added to the functions provided in Fl_Vk_Window.

   4.1 Extensions

   Instance extensions must be listed in:

       - Fl_Vk_Window's instance extensions are listed and returned in a vector as get_instance_extensions and get_optional_extensions.

       - Device extensions:
       Fl_Vk_Window's device extensions are similarly listed in get_device_extensions.

       Besides the extensions, for drawing to the OpenGL window, Fl_Vk_Window should create a renderPass and pipeline and store them in m_renderPass and m_pipeline, albeit these are entirely optional.

   4.2 Flow of the Window creation and drawing:

       Fl_Vk_Window calls the overloaded extensions files.
       The protected init_vk function is called to intialize the Fl_Vk_Context.
       The potentially overloaded init_colorspace can be used to select a color space and swapchain format.  By default, init_colorspace will try to use the best format.
       Fl_Vk_Window will call prepare() which you must overload to create the minimal primitives for your window (render pass, pipeline, shaders, etc).
       Fl_Vk_Window will call vk_draw_begin() where you can set the background and stencil/depth values.
       Fl_Vk_Window will "finally" call draw(), where you will draw all your drawing.  It is suggested that draw() use getCurrentCommandBuffer() to get a command buffer for a Vulkan MAX_FRAMES_IN_FLIGHT structure.
       Fl_Vk_Window will then finally call vk_draw_end().  You usually don't need to change it.
       On resizes or hiding of the window, Fl_Vk_Window will tear down all of its internal Vulkan primitives and call Fl_Vk_Window's overloaded destroy() function so you can do the same for primitives you created in prepare().
       
5  Demos
--------

vk_shape   - similar to FLTK's OpenGL shape.  Draws a triangle/circle.
vk_texture - Like vk_shape but with a simple texture.  It also tests depth/stencil.
vk_cube    - similar to FLTK's OpenGL cube.  Requires the glm math library.

6 Projects
----------

For projects using this Vulkan fork of FLTK, please take a look at:

https://github.com/ggarra13/mrv2
HDR video player (vmrv2) - Relative mature.

https://github.com/ggarra13/usdviewer
OpenUSD Scanline Renderer (Early WIP)
