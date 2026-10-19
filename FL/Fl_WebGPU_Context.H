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

//! Wrapper to keep a queue and a mutex for safe use in multithreading.
//! Each FLTK window receives a pointer to it.
struct WebGPUQueue
{
    std::mutex                    mutex;
    WGPUQueue                     queue = 0;
};

struct WebGPUContext
{
    WGPUInstance instance = 0;
    WGPUDevice device = 0;
    WebGPUGPUQueue safe_thread_queue;
    WGPUSurface surface = 0;
    WGPURenderPipeline pipeline = 0;

    WGPUCommandEncoder cmd_encoder = 0;
    WGPURenderPassEncoder renderPass = 0;

    // Thread safe queue (use lock_guard on mutex when submitting)
    WGPUQueue& queue() { return safe_thread_queue.queue; }
    std::mutex& queue_mutex() { return safe_thread_queue.mutex; }
};

#endif // Fl_WebGPU_Context_H
