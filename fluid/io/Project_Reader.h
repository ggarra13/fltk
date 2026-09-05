//
// Fluid Project File Reader header for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2025 by Bill Spitzak and others.
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

#ifndef FLUID_IO_PROJECT_READER_H
#define FLUID_IO_PROJECT_READER_H

#include "nodes/Node.h"

#include <FL/fl_attr.h>

#include <stdio.h>


class Node;

namespace fluid {

class Project;

namespace io {

extern int fdesign_flip;

int read_file(Project &proj, const std::string& filename, int merge, Strategy strategy=Strategy::FROM_FILE_AS_LAST_CHILD);

class Project_Reader
{
protected:
  /// Link Project_Reader class to the project.
  Project &proj_;

  /// Project input file
  FILE *fin = nullptr;
  /// Number of most recently read line
  int lineno = 0;
  /// The file path and name
  std::string fname { };

  int nextchar() { for (;;) { int ret = fgetc(fin); if (ret!='\r') return ret; } }
  int skip_to_word();

public:
  /// Holds the file version number after reading the "version" tag
  double read_version = 0.0;

public:
  Project_Reader(Project &proj);
  ~Project_Reader();
  int open_read(const std::string& s);
  int close_read();
  std::string filename_name() const;
  int read_quoted();
  Node *read_children(Node *p, int merge, Strategy strategy, char skip_options=0);
  int read_project(const std::string& filename, int merge, Strategy strategy=Strategy::FROM_FILE_AS_LAST_CHILD);
  void read_error(const char *format, ...);
  bool more_words();
  std::string read_word(int wantbrace = 0);
  int read_int();
  int read_fdesign_line(std::string& name, std::string& value);
  void read_fdesign();
  int current_line_number() const { return lineno; }
};

} // namespace io
} // namespace fluid

#endif // FLUID_IO_PROJECT_READER_H
