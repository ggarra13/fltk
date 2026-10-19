//
// WebGPU header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2023 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

/* \file
   Fl_WebGPU_Context struct . */

#ifndef Fl_WebGPU_Context_H
#define Fl_WebGPU_Context_H

#include <webgpu/webgpu.h>

struct WebGPUContext
{
    WGPUInstance instance;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPURenderPipeline pipeline;
};

#endif // Fl_WebGPU_Context_H
