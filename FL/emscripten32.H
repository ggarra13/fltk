//
// Emscripten platform header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2024 by Bill Spitzak and others.
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

#ifndef FL_PLATFORM_H
#  error "Never use <FL/emscripten32.H> directly; include <FL/platform.H> instead."
#endif // !FL_PLATFORM_H

#include <FL/Enumerations.H>
#include <emscripten/val.h>

typedef int Window;

extern FL_EXPORT Window fl_em_xid(const Fl_Window *win);
extern FL_EXPORT Fl_Window *fl_em_find(Window);
extern FL_EXPORT emscripten::EM_VAL fl_em_gc();
// Read text from the associated file handle in empath to a char pointer.
// This char pointer requires freeing using ::free.
extern FL_EXPORT char *fl_read_to_string(const char *empath);
// Read binary data from the associated file handle in empath to a uchar pointer.
// This uchar pointer requires deletion using delete[].
extern FL_EXPORT const uchar *fl_read_to_binary(const char *empath, int &len);
// Write data to the associated file handle in empath.
extern FL_EXPORT int fl_write_to_file(const char *empath, const uchar *data, int len);