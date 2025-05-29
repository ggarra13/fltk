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

sudo apt install libvulkan-dev.

3.2 Fedora
----------


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

VULKAN_SDK should be set to:

/usr/local    on Intel
/opt/homebrew on M1+

If you installed the version from LunarG, by default it will get installed in your home directory, like:

/home/<username>/VulkanSDK/<SDK version>/macOS
