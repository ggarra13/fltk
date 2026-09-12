//
// Fluid Project File Reader code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2026 by Bill Spitzak and others.
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

// You may find the basic read_* and write_* routines to
// be useful for other programs.  I have used them many times.
// They are somewhat similar to tcl, using matching { and }
// to quote strings.

#include "io/Project_Reader.h"

#include "Fluid.h"
#include "Project.h"
#include "message.h"
#include "app/shell_command.h"
#include "proj/undo.h"
#include "app/Snap_Action.h"
#include "nodes/factory.h"
#include "nodes/Function_Node.h"
#include "nodes/Widget_Node.h"
#include "nodes/Grid_Node.h"
#include "nodes/Window_Node.h"
#include "nodes/Menu_Node.h"
#include "widgets/Node_Browser.h"
#include "../../src/flstring.h"

#include <FL/Fl_Window.H>
#include <FL/fl_message.H>

/// \defgroup flfile .fl Project File Operations
/// \{

using namespace fluid;
using namespace fluid::io;

// This file contains code to read and write .fl files.

/// If set, we read an old fdesign file and widget y coordinates need to be flipped.
int fluid::io::fdesign_flip = 0;

/** \brief Read a .fl project file.

 The .fl file format is documented in `fluid/README_fl.txt`.

 \param[in] filename read this file
 \param[in] merge if this is set, merge the file into an existing project
    at Fluid.proj.tree.current
 \param[in] strategy add new nodes after current or as last child
 \return 0 if the operation failed, 1 if it succeeded
 */
int fluid::io::read_file(Project &proj, const std::string& filename, int merge, Strategy strategy) {
  Project_Reader f(proj);
  strategy.source(Strategy::FROM_FILE);
  return f.read_project(filename, merge, strategy);
}

/**
 Convert a single ASCII char, assumed to be a hex digit, into its decimal value.
 \param[in] x ASCII character
 \return decimal value or 20 if character is not a valid hex digit (0..9,a..f,A..F)
 */
static int hexdigit(int x) {
  if ((x < 0) || (x > 127)) return 20;
  if (fl_ascii_isdigit(x)) return x-'0';
  if (fl_ascii_isupper(x)) return x-'A'+10;
  if (fl_ascii_islower(x)) return x-'a'+10;
  return 20;
}

// ---- Project_Reader ---------------------------------------------- MARK: -

/** \brief Construct local project reader. */
Project_Reader::Project_Reader(Project &proj)
: proj_(proj)
{
}

/** \brief Release project reader resources. */
Project_Reader::~Project_Reader()
{
}

/**
 Open an .fl file for reading.
 \param[in] s filename, if nullptr, read from stdin instead
 \return 0 if the operation failed, 1 if it succeeded
 */
int Project_Reader::open_read(const std::string& s) {
  lineno = 1;
  if (s.empty()) {
    fin = stdin;
    fname = "stdin";
  } else {
    FILE *f = fl_fopen(s.c_str(), "rb");
    if (!f)
      return 0;
    fin = f;
    fname = s;
  }
  return 1;
}

/**
 Close the .fl file.
 \return 0 if the operation failed, 1 if it succeeded
 */
int Project_Reader::close_read() {
  if (fin == nullptr) {
    return 1;
  }
  if (fin != stdin) {
    int x = fclose(fin);
    fin = nullptr;
    return x >= 0;
  }
  return 1;
}

/**
 Return the name part of the current filename and path.
 \return a pointer into a string that is not owned by this class
 */
std::string Project_Reader::filename_name() const {
  return fl_filename_name_str(fname);
}

/**
 Convert an ASCII sequence from the \.fl file following a previously read `\\` into a single character.
 Conversion includes the common C style \\ characters like \\n, \\x## hex
 values, and \\o### octal values.
 \return a character in the ASCII range
 */
int Project_Reader::read_quoted() {      // read whatever character is after a \ .
  int c,d,x;
  switch(c = nextchar()) {
    case '\n': lineno++; return -1;
    case 'a' : return('\a');
    case 'b' : return('\b');
    case 'f' : return('\f');
    case 'n' : return('\n');
    case 'r' : return('\r');
    case 't' : return('\t');
    case 'v' : return('\v');
    case 'x' :    /* read hex */
      for (c=x=0; x<3; x++) {
        int ch = nextchar();
        d = hexdigit(ch);
        if (d > 15) {ungetc(ch,fin); break;}
        c = (c<<4)+d;
      }
      break;
    default:              /* read octal */
      if (c<'0' || c>'7') break;
      c -= '0';
      for (x=0; x<2; x++) {
        int ch = nextchar();
        d = hexdigit(ch);
        if (d>7) {ungetc(ch,fin); break;}
        c = (c<<3)+d;
      }
      break;
  }
  return(c);
}

/**
 Recursively read child nodes in the .fl design file.

 If this is the first call, also read the global settings for this design.

 \param[in] p parent node or nullptr
 \param[in] merge if set, merge into existing design, else replace design
 \param[in] strategy add nodes after current or as last child
 \param[in] skip_options this is set if the options were already found in
 a previous call, and there is no need to waste time searching for them.
 \return the last node that was created
 */
Node *Project_Reader::read_children(Node *p, int merge, Strategy strategy, char skip_options) {
  Fluid.proj.tree.current = p;
  Node *last_child_read = nullptr;
  Node *t = nullptr;
  std::string c;
  for (;;) {
    if (!more_words()) {
      if (p && !merge)
        read_error("Missing '}' in line %d", lineno);
      break;
    }
    c = read_word();
  REUSE_C:
    if (c == "}") {
      if (!p) read_error("Unexpected '}' in line %d", lineno);
      break;
    }

    // Make sure that we don't go through the list of options for child nodes
    if (!skip_options) {
      // this is the first word in a .fd file:
      if (c == "Magic:") {
        read_fdesign();
        return nullptr;
      }

      if (c == "version") {
        c = read_word();
        read_version = strtod(c.c_str(),nullptr);
        if (read_version<=0 || read_version>double(FL_VERSION+0.000021))
          read_error(
            "Project file version '%s' is newer than this version of Fluid\n"
            "Some features may not be supported.", c.c_str());
        continue;
      }

      // back compatibility with Vincent Penne's original class code:
      if (!p && c == "define_in_struct") {
        Node *t = add_new_widget_from_file("class", Strategy::FROM_FILE_AS_LAST_CHILD);
        t->name(read_word());
        Fluid.proj.tree.current = p = t;
        merge = 1; // stops "missing }" error
        continue;
      }

      if (c == "do_not_include_H_from_C") {
        proj_.include_H_from_C=0;
        goto CONTINUE;
      }
      if (c == "use_FL_COMMAND") {
        proj_.use_FL_COMMAND=1;
        goto CONTINUE;
      }
      if (c == "utf8_in_src") {
        proj_.utf8_in_src=1;
        goto CONTINUE;
      }
      if (c == "avoid_early_includes") {
        proj_.avoid_early_includes=1;
        goto CONTINUE;
      }
      if (c.compare(0, 5, "i18n_") == 0) {
        proj_.i18n.read(*this, c);
        goto CONTINUE;
      }
      if (c == "header_name") {
        if (!proj_.header_file_set) proj_.header_file_name = read_word();
        else read_word();
        goto CONTINUE;
      }

      if (c == "code_name") {
        if (!proj_.code_file_set) proj_.code_file_name = read_word();
        else read_word();
        goto CONTINUE;
      }

      if (c == "strings_name") {
        if (!proj_.strings_file_set) proj_.strings_file_name = read_word();
        else read_word();
        goto CONTINUE;
      }

      if (c == "include_guard") {
        proj_.include_guard = read_word();
        goto CONTINUE;
      }

      if (c == "snap") {
        Fluid.layout_list.read(this);
        goto CONTINUE;
      }

      if (c == "gridx" || c == "gridy") {
        // grid settings are now global
        read_word();
        goto CONTINUE;
      }

      if (c == "shell_commands") {
        if (g_shell_config) {
          g_shell_config->read(this);
        } else {
          read_word();
        }
        goto CONTINUE;
      }

      if (c == "mergeback") {
        proj_.write_mergeback_data = read_int();
        goto CONTINUE;
      }
    }
    t = add_new_widget_from_file(c, strategy);
    if (!t) {
      if (c.size() > 32)
        read_error("Unknown word \"%.32s...\" in line %d", c.c_str(), lineno);
      else
        read_error("Unknown word \"%s\" in line %d", c.c_str(), lineno);
      continue;
    }
    last_child_read = t;
    // After reading the first widget, we no longer need to look for options
    skip_options = 1;

    t->name(read_word());

    c = read_word(1);
    // There can actually be two keywords here. The first one used to be a
    // "prefix", i.e. class attributes.
    if (c != "{" && t->is_class()) {   // <prefix> <name>
      ((Class_Node*)t)->prefix( t->name() );
      t->name( c );
      c = read_word(1);
    }

    if (c != "{") {
      read_error("Missing property list for '%.32s' in line %d",t->title().c_str(), lineno);
      goto REUSE_C;
    }

    t->folded_ = 1;
    for (;;) {
      std::string cc = read_word();
      if (cc == "}") break;
      t->read_property(*this, cc);
    }

    if (t->can_have_children()) {
      c = read_word(1);
      if (c != "{") {
        read_error("Missing child list for '%.32s' in line %d",t->title().c_str(), lineno);
        goto REUSE_C;
      }
      read_children(t, 0, Strategy::FROM_FILE_AS_LAST_CHILD, skip_options);
      t->postprocess_read();
      // FIXME: this has no business in the file reader!
      // TODO: this is called whenever something is pasted from the top level into a grid
      //    It makes sense to make this more universal for other widget types too.
      if (merge && t && t->parent && dynamic_cast<Grid_Node*>(t->parent)) {
        if (Window_Node::popupx != 0x7FFFFFFF) {
          ((Grid_Node*)t->parent)->insert_child_at(((Widget_Node*)t)->o, Window_Node::popupx, Window_Node::popupy);
        } else {
          ((Grid_Node*)t->parent)->insert_child_at_next_free_cell(((Widget_Node*)t)->o);
        }
      }

      t->layout_widget();
    }

    if (strategy.placement() == Strategy::AS_FIRST_CHILD) {
      strategy.placement(Strategy::AFTER_CURRENT);
    }
    if (strategy.placement() == Strategy::AFTER_CURRENT) {
      Fluid.proj.tree.current = t;
    } else {
      Fluid.proj.tree.current = p;
    }

  CONTINUE:;
  }
  if (merge && last_child_read && last_child_read->parent) {
    last_child_read->parent->postprocess_read();
    last_child_read->parent->layout_widget();
  }
  return last_child_read;
}

/** \brief Read a .fl project file.
 \param[in] filename read this file
 \param[in] merge if this is set, merge the file into an existing project
 at Fluid.proj.tree.current
 \param[in] strategy add new nodes after current or as last child
 \return 0 if the operation failed, 1 if it succeeded
 */
int Project_Reader::read_project(const std::string& filename, int merge, Strategy strategy) {
  Node *o;
  proj_.undo.suspend();
  read_version = 0.0;
  if (!open_read(filename)) {
    proj_.undo.resume();
    return 0;
  }
  if (merge)
    deselect();
  else
    proj_.reset();

  try {
    read_children(Fluid.proj.tree.current, merge, strategy);
  } catch (const fluid::UserCanceledException&) {
    // User chose abort - silently return or log
  } catch (const fluid::ReadException& e) {
    fluid_alert("Error reading file: %s", e.what());
  }

  // clear this
  Fluid.proj.tree.current = nullptr;
  // Force menu items to be rebuilt...
  for (o = Fluid.proj.tree.first; o; o = o->next) {
    if (dynamic_cast<Menu_Manager_Node*>(o)) {
      o->add_child(nullptr,nullptr);
    }
  }
  for (o = Fluid.proj.tree.first; o; o = o->next) {
    if (o->selected) {
      Fluid.proj.tree.current = o;
      break;
    }
  }
  selection_changed(Fluid.proj.tree.current);
  if (g_shell_config) {
    g_shell_config->rebuild_shell_menu();
    g_shell_config->update_settings_dialog();
  }
  Fluid.layout_list.update_dialogs();
  proj_.update_settings_dialog();
  int ret = close_read();
  proj_.undo.resume();
  return ret;
}

/**
 Display an error while reading the file.
 If the .fl file isn't opened for reading, pop up an FLTK dialog, otherwise
 print to stdout.
 \param[in] format printf style format string, followed by an argument list
 */
void Project_Reader::read_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  if (fluid_choice("Fluid: ERROR reading project file", format, "Abort", "Ignore", nullptr, args) == msg::ABORT) {
    throw fluid::UserCanceledException();
  }
  va_end(args);
}

/**
 Skip whitespace and comments and return the first significant character.

 This is the shared first step of more_words() and read_word(): it consumes
 all comments (# to end of line) and whitespace, tracking line numbers, and
 leaves the file position right after the first character that is neither.

 \return the first significant character, or -1 at EOF
 */
int Project_Reader::skip_to_word() {
  int x;
  for (;;) {
    x = nextchar();
    if (x < 0 && feof(fin)) {   // eof
      return -1;
    } else if (x == '#') {      // comment
      do x = nextchar(); while (x >= 0 && x != '\n');
      lineno++;
      continue;
    } else if (x == '\n') {
      lineno++;
    } else if (!fl_ascii_isspace(x)) {
      return x;
    }
  }
}

/**
 Check whether another word is available before the end of the file.

 This skips whitespace and comments exactly like read_word() does, but does
 not consume the word itself, so it can be used to tell a genuine, expected
 end of file (there is no more content to read) apart from an unexpected
 EOF in the middle of a bracketed structure, which read_word() treats as an
 error. This is only meaningful at the top level of the file; everywhere
 else, the surrounding braces make an EOF always an error.

 \return true if there is at least one more word before EOF
 */
bool Project_Reader::more_words() {
  int x = skip_to_word();
  if (x < 0) return false;
  ungetc(x, fin);
  return true;
}

/**
 Return a word read from the .fl file.

 This will skip all comments (# to end of line), and evaluate
 all \\xxx sequences and use \\ at the end of line to remove the newline.

 A word is any one of:
 - a continuous string of non-space chars except { and } and #
 - everything between matching {...} (unless wantbrace != 0)
 - the characters '{' and '}'

 \param[in] wantbrace if set, reading a `{` as the first non-space character
    will return the string `"{"`, if clear, a `{` is seen as the start of a word
 \return the word that was read. If wantbrace is not set, but we read a
    leading '{', the returned string will be stripped of its leading and
    trailing braces.
 \throw fluid::ReadException if the file ends before a word can be read.
    Call more_words() first if EOF here would be a normal, expected
    end of file rather than a corrupt or truncated project.
 */
std::string Project_Reader::read_word(int wantbrace) {
  int x = skip_to_word();
  if (x < 0) {
    throw fluid::ReadException("Unexpected end of file in line " + std::to_string(lineno));
  }

  if (x == '{' && !wantbrace) {

    // read in whatever is between braces
    std::string word;
    int nesting = 0;
    for (;;) {
      x = nextchar();
      if (x<0) {read_error("Missing '}' in line %d", lineno); break;}
      else if (x == '#') { // embedded comment
        do x = nextchar(); while (x >= 0 && x != '\n');
        lineno++;
        continue;
      } else if (x == '\n') lineno++;
      else if (x == '\\') {x = read_quoted(); if (x<0) continue;}
      else if (x == '{') nesting++;
      else if (x == '}') {if (!nesting--) break;}
      word.push_back((char)x);
    }
    return word;

  } else if (x == '{' || x == '}') {
    // all the punctuation is a word:
    return std::string(1, (char)x);

  } else {

    // read in an unquoted word:
    std::string word;
    for (;;) {
      if (x == '\\') {x = read_quoted(); if (x<0) continue;}
      else if (x<0 || fl_ascii_isspace(x) || x=='{' || x=='}' || x=='#') break;
      word.push_back((char)x);
      x = nextchar();
    }
    ungetc(x, fin);
    return word;

  }
}

/** Read a word and interpret it as an integer value.
 \return integer value, or 0 if the word is not an integer
 */
int Project_Reader::read_int() {
  return atoi(read_word().c_str());
}

/** Read fdesign name/value pairs.
 Fdesign is the file format of the XForms UI designer. It stores lists of name
 and value pairs separated by a colon: `class: FL_LABELFRAME`.
 \param[out] name string
 \param[out] value string
 \return 0 if end of file, else 1
 */
int Project_Reader::read_fdesign_line(std::string& name, std::string& value) {
  int x;
  name.clear();
  // find a colon:
  for (;;) {
    x = nextchar();
    if (x < 0 && feof(fin)) return 0;
    if (x == '\n') {name.clear(); continue;} // no colon this line...
    if (!fl_ascii_isspace(x)) {
      name.push_back((char)x);
    }
    if (x == ':') break;
  }
  name.pop_back(); // drop the trailing ':'

  // skip to start of value:
  for (;;) {
    x = nextchar();
    if ((x < 0 && feof(fin)) || x == '\n' || !fl_ascii_isspace(x)) break;
  }

  // read the value:
  value.clear();
  for (;;) {
    if (x == '\\') {x = read_quoted(); if (x<0) continue;}
    else if (x == '\n') break;
    value.push_back((char)x);
    x = nextchar();
  }
  return 1;
}

/// Lookup table from fdesign .fd files to .fl files
static const char *class_matcher[] = {
  "FL_CHECKBUTTON", "Fl_Check_Button",
  "FL_ROUNDBUTTON", "Fl_Round_Button",
  "FL_ROUND3DBUTTON", "Fl_Round_Button",
  "FL_LIGHTBUTTON", "Fl_Light_Button",
  "FL_FRAME", "Fl_Box",
  "FL_LABELFRAME", "Fl_Box",
  "FL_TEXT", "Fl_Box",
  "FL_VALSLIDER", "Fl_Value_Slider",
  "FL_MENU", "Fl_Menu_Button",
  "3", "FL_BITMAP",
  "1", "FL_BOX",
  "71","FL_BROWSER",
  "11","FL_BUTTON",
  "4", "FL_CHART",
  "42","FL_CHOICE",
  "61","FL_CLOCK",
  "25","FL_COUNTER",
  "22","FL_DIAL",
  "101","FL_FREE",
  "31","FL_INPUT",
  "12","Fl_Light_Button",
  "41","FL_MENU",
  "23","FL_POSITIONER",
  "13","Fl_Round_Button",
  "21","FL_SLIDER",
  "2", "FL_BOX", // was FL_TEXT
  "62","FL_TIMER",
  "24","Fl_Value_Slider",
  nullptr};


/**
 Finish a group of widgets and optionally transform its children's coordinates.

 Implements the same functionality as Fl_Group::forms_end() from the forms
 compatibility library would have done:

 - resize the group to surround its children if the group's w() == 0
 - optionally flip the \p y coordinates of all children relative to the group's window
 - Fl_Group::end() the group

 \note Copied from forms_compatibility.cxx and modified as a static fluid
 function so we don't have to link to fltk_forms.

 \param[in]  g     the Fl_Group widget
 \param[in]  flip  flip children's \p y coordinates if true (non-zero)
 */
static void forms_end(Fl_Group *g, int flip) {
  // set the dimensions of a group to surround its contents
  const int nc = g->children();
  if (nc && !g->w()) {
    Fl_Widget*const* a = g->array();
    Fl_Widget* o = *a++;
    int rx = o->x();
    int ry = o->y();
    int rw = rx+o->w();
    int rh = ry+o->h();
    for (int i = nc - 1; i--;) {
      o = *a++;
      if (o->x() < rx) rx = o->x();
      if (o->y() < ry) ry = o->y();
      if (o->x() + o->w() > rw) rw = o->x() + o->w();
      if (o->y() + o->h() > rh) rh = o->y() + o->h();
    }
    g->Fl_Widget::resize(rx, ry, rw-rx, rh-ry);
  }
  // flip all the children's coordinate systems:
  if (nc && flip) {
    Fl_Widget* o = (g->as_window()) ? g : g->window();
    int Y = o->h();
    Fl_Widget*const* a = g->array();
    for (int i = nc; i--;) {
      Fl_Widget* ow = *a++;
      int newy = Y - ow->y() - ow->h();
      ow->Fl_Widget::resize(ow->x(), newy, ow->w(), ow->h());
    }
  }
  g->end();
}

/**
 Read a XForms design file.
 .fl and .fd file start with the same header. Fluid can recognize .fd XForms
 Design files by a magic number. It will read them and map XForms widgets onto
 FLTK widgets.
 \see http://xforms-toolkit.org
 */
void Project_Reader::read_fdesign() {
  int fdesign_magic = read_int();
  fdesign_flip = (fdesign_magic < 13000);
  Widget_Node *window = nullptr;
  Widget_Node *group = nullptr;
  Widget_Node *widget = nullptr;
  if (!Fluid.proj.tree.current) {
    Node *t = add_new_widget_from_file("Function", Strategy::FROM_FILE_AS_LAST_CHILD);
    t->name("create_the_forms()");
    Fluid.proj.tree.current = t;
  }
  std::string name, value;
  for (;;) {
    if (!read_fdesign_line(name, value)) break;

    if (name == "Name") {

      window = (Widget_Node*)add_new_widget_from_file("Fl_Window", Strategy::FROM_FILE_AS_LAST_CHILD);
      window->name(value);
      window->label(value);
      Fluid.proj.tree.current = widget = window;

    } else if (name == "class") {

      if (value == "FL_BEGIN_GROUP") {
        group = widget = (Widget_Node*)add_new_widget_from_file("Fl_Group", Strategy::FROM_FILE_AS_LAST_CHILD);
        Fluid.proj.tree.current = group;
      } else if (value == "FL_END_GROUP") {
        if (group) {
          Fl_Group* g = (Fl_Group*)(group->o);
          g->begin();
          forms_end(g, fdesign_flip);
          Fl_Group::current(nullptr);
        }
        group = widget = nullptr;
        Fluid.proj.tree.current = window;
      } else {
        for (int i = 0; class_matcher[i]; i += 2)
          if (value == class_matcher[i]) {
            value = class_matcher[i+1]; break;}
        widget = (Widget_Node*)add_new_widget_from_file(value, Strategy::FROM_FILE_AS_LAST_CHILD);
        if (!widget) {
          fluid_message("class %s not found, using Fl_Button\n", value.c_str());
          widget = (Widget_Node*)add_new_widget_from_file("Fl_Button", Strategy::FROM_FILE_AS_LAST_CHILD);
        }
      }

    } else if (widget) {
      if (!widget->read_fdesign(name, value))
        fluid_message("Ignoring \"%s: %s\"\n", name.c_str(), value.c_str());
    }
  }
}

/// \}
