//
// FLTK native file chooser widget wrapper for the web browser's file chooser
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

#include <FL/Fl_Native_File_Chooser.H>
#include <emscripten.h>
#include <emscripten/val.h>
#include <vector>
#include <string>

using namespace emscripten;

static std::vector<val> filehandles;

// This is a simplistic function for parsing filters
// The browser's file dialog expects filters of the form: `'application/*': ['.exe', '.zip',
// '.rar']`. This function doesn't generate mime types based on file extensions. It takes a filter
// like `Text\t*.txt\nC File\t*.c\n" and produces: `.txt .c`. The string is then split in the
// javascipt side to generate an array of extensions.
static char *parse_filter(const char *f) {
  char *parsed = new char[strlen(f) * 2];
  parsed[0] = '\0';

  const char *p = f;
  while (*p) {
    if (*p == '*') {
      p++;
      while (*p != '\t' && *p != '\n' && *p != '\0') {
        strncat(parsed, p, 1);
        p++;
      }
      strcat(parsed, " ");
    }
    p++;
  }

  // Remove trailing space
  size_t len = strlen(parsed);
  if (len > 0 && parsed[len - 1] == ' ') {
    parsed[len - 1] = '\0';
  }
  return parsed;
}

// The Fl_Native_File_Chooser allows accessing the local file system via the [local file system access api](https://developer.mozilla.org/en-US/docs/Web/API/File_System_API),
// which currently is only supported by chrome and bing.
// However note that since emscripten applications are sandboxed,
// the filenames returned cannot be used with fopen or with functions expecting a path such as image
// constructors. FLTK provides 2 primary functions for reading files chosen via the browser's file
// chooser:
// 1- fl_read_to_string(const char *empath) which allows reading a file to string through
// the stored handle.
// 2- fl_read_to_binary(const char *empath, int &len) which allows to read a
// binary file to a uchar pointer.
// 3- fl_writer_to_file(const char *empath, const uchar *data, int len).
// Alternatively, you can use FLTK's own file chooser: Fl_File_Chooser, which allows accessing
// the virtual file system compiled by emscripten into a <binary>.data file. It also allows reading
// from the file directly with fopen and passing the path to FLTK functions/methods requiring a path
// like image constructors.
class Fl_Emscripten_Native_File_Chooser_Driver : public Fl_Native_File_Chooser_Driver {
  friend class Fl_Native_File_Chooser;
  int _btype;
  char *_filter;
  char *_dir;
  char *_preset_file;
  std::vector<std::string> files;
  Fl_Emscripten_Native_File_Chooser_Driver(int val);
  ~Fl_Emscripten_Native_File_Chooser_Driver();
  void type(int t) FL_OVERRIDE;
  int type() const FL_OVERRIDE;
  int count() const FL_OVERRIDE;
  const char *filename() const FL_OVERRIDE;
  const char *filename(int i) const FL_OVERRIDE;
  void directory(const char *) FL_OVERRIDE;
  int show() FL_OVERRIDE;
  void filter(const char *f) FL_OVERRIDE;
  void preset_file(const char *f) FL_OVERRIDE;
  const char *preset_file() const FL_OVERRIDE;
};

Fl_Native_File_Chooser::Fl_Native_File_Chooser(int val) {
  platform_fnfc = new Fl_Emscripten_Native_File_Chooser_Driver(val);
}

Fl_Emscripten_Native_File_Chooser_Driver::Fl_Emscripten_Native_File_Chooser_Driver(int val)
  : Fl_Native_File_Chooser_Driver(val)
  , _btype(val)
  , _filter(NULL)
  , _dir(NULL)
  , _preset_file(NULL) {}

int Fl_Emscripten_Native_File_Chooser_Driver::type() const {
  return _btype;
}

void Fl_Emscripten_Native_File_Chooser_Driver::type(int val) {
  _btype = val;
}

// This translates the chooser type to a browser picker. We have 3 main types:
// 1- showOpenFilePicker
// 2- showSaveFilePicker
// 3- showDirectoryPicker
// clang-format off
EM_ASYNC_JS(EM_VAL, showChooser, (int type, const char *filter, const char *dir, const char *preset), {
  if (!window.showOpenFilePicker) {
    return null;
  }
  let multiple = false;
  let files = false;
  let save = false;
  if (type === 0 || type === 2 || type === 4) {
    files = true;
  }
  if (type === 2 || type === 3) {
    mutliple = true;
  }
  if (type > 3) {
    save = true;
  }
  let func;
  if (files) {
    if (save) {
      func = window.showSaveFilePicker;
    } else {
      func = window.showOpenFilePicker;
    }
  } else {
    func = window.showDirectoryPicker;
  }
  let dir1 = dir ? UTF8ToString(dir) : 'desktop';
  let filt = UTF8ToString(filter).split(' ');
  // I use application/x-abiword since I don't think it's widely used as a mime type!
  const openPickerOpts = {
    types: [
      {
        accept: {
          "application/x-abiword": filt,
        },
      },
    ],
    startIn: dir1,
    excludeAcceptAllOption: true,
    multiple: multiple,
  };
  const savePickerOpts = {
    types: [
      {
        accept: {
          "application/x-abiword": filt,
        },
      },
    ],
    suggestedName: UTF8ToString(preset),
    startIn: dir1,
    excludeAcceptAllOption: true,
  };
  const directoryOpts = {
    mode: "readwrite",
    startIn: dir1,
  };
  if (files) {
    return Emval.toHandle(func(save ? savePickerOpts : openPickerOpts));
  } else {
    return Emval.toHandle(func(directoryOpts));
  }
});
// clang-format on

int Fl_Emscripten_Native_File_Chooser_Driver::show() {
  EM_VAL chooser = showChooser((int)_btype, _filter, _dir, _preset_file);
  if (!chooser)
    return -1;
  val promise = val::take_ownership(chooser);
  val filehandles0 = promise.await();
  switch (_btype) {
    case Fl_Native_File_Chooser::BROWSE_FILE:
    case Fl_Native_File_Chooser::BROWSE_MULTI_FILE: {
      // Some pickers return an array while others return a single handle
      filehandles = vecFromJSArray<val>(filehandles0);
      files.clear();
      for (int i = 0; i < filehandles.size(); i++) {
        files.push_back(filehandles[i]["name"].as<std::string>());
      }
      break;
    }
    case Fl_Native_File_Chooser::BROWSE_SAVE_FILE:
    case Fl_Native_File_Chooser::BROWSE_DIRECTORY:
    case Fl_Native_File_Chooser::BROWSE_MULTI_DIRECTORY:
    case Fl_Native_File_Chooser::BROWSE_SAVE_DIRECTORY: {
      filehandles = {filehandles0};
      files.clear();
      files.push_back(filehandles[0]["name"].as<std::string>());
      break;
    }
  }
  return 0;
}

Fl_Emscripten_Native_File_Chooser_Driver::~Fl_Emscripten_Native_File_Chooser_Driver() {
  strfree(_filter);
  strfree(_dir);
  strfree(_preset_file);
}

void Fl_Emscripten_Native_File_Chooser_Driver::filter(const char *f) {
  _filter = strfree(_filter);
  _filter = parse_filter(f);
}

void Fl_Emscripten_Native_File_Chooser_Driver::directory(const char *f) {
  _dir = strfree(_dir);
  _dir = strnew(f);
}

void Fl_Emscripten_Native_File_Chooser_Driver::preset_file(const char *val) {
  _preset_file = strfree(_preset_file);
  _preset_file = strnew(val);
}

const char *Fl_Emscripten_Native_File_Chooser_Driver::preset_file() const {
  return _preset_file;
}

int Fl_Emscripten_Native_File_Chooser_Driver::count() const {
  return files.size();
}

const char *Fl_Emscripten_Native_File_Chooser_Driver::filename() const {
  if (files.size() > 0)
    return files[0].c_str();
  else
    return NULL;
}

const char *Fl_Emscripten_Native_File_Chooser_Driver::filename(int i) const {
  if (files.size() > i)
    return files[i].c_str();
  else
    return NULL;
}

static int find_index(const char *str) {
  int ret = -1;
  for (int i = 0; i < filehandles.size(); i++) {
    if (filehandles[i]["name"].as<std::string>() == str) {
      ret = i;
      break;
    }
  }
  return ret;
}

char *fl_read_to_string(const char *empath) {
  int idx = find_index(empath);
  if (idx == -1)
    return NULL;
  val file = filehandles[idx];
  val data = file.call<val>("getFile").await();
  val text = data.call<val>("text").await();
  return strdup(text.as<std::string>().c_str());
}

uchar *fl_read_to_binary(const char *empath, int &len) {
  int idx = find_index(empath);
  if (idx == -1)
    return NULL;
  val file = filehandles[idx];
  val data = file.call<val>("getFile").await();
  val bytes = data.call<val>("arrayBuffer").await();
  std::vector<uchar> vec = vecFromJSArray<uchar>(val::global("Uint8Array").new_(bytes));
  uchar *ret = new uchar[vec.size()];
  len = vec.size();
  memcpy(ret, vec.data(), vec.size());
  return ret;
}

int fl_write_to_file(const char *empath, const uchar *data, int len) {
  int idx = find_index(empath);
  if (idx == -1)
    return -1;
  val data1 = val(typed_memory_view(len, data));
  val file = filehandles[idx];
  val writable = file.call<val>("createWritable").await();
  writable.call<val>("write", data1).await();
  writable.call<val>("close").await();
  return 0;
}