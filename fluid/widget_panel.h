//
// Widget panel for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
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

// generated by Fast Light User Interface Designer (fluid) version 1.0400

#ifndef widget_panel_h
#define widget_panel_h
#include <FL/Fl.H>
#include "custom_widgets.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
extern void propagate_load(Fl_Group*, void*);
#include <FL/Fl_Input.H>
extern void label_cb(Fl_Input*, void*);
#include <FL/Fl_Choice.H>
extern Fl_Menu_Item labeltypemenu[];
extern void labeltype_cb(Fl_Choice*, void*);
extern void image_cb(Fl_Input*, void*);
#include <FL/Fl_Button.H>
extern void image_browse_cb(Fl_Button*, void*);
#include "pixmaps.h"
extern void compress_image_cb(Fl_Button*, void*);
extern void bind_image_cb(Fl_Button*, void*);
extern void inactive_cb(Fl_Input*, void*);
extern void inactive_browse_cb(Fl_Button*, void*);
extern void compress_deimage_cb(Fl_Button*, void*);
extern void bind_deimage_cb(Fl_Button*, void*);
extern void align_cb(Fl_Button*, void*);
extern void align_text_image_cb(Fl_Choice*, void*);
extern void align_position_cb(Fl_Choice*, void*);
#include <FL/Fl_Box.H>
extern void position_group_cb(Fl_Group*, void*);
extern void x_cb(Fluid_Coord_Input*, void*);
extern Fluid_Coord_Input *widget_x_input;
extern void y_cb(Fluid_Coord_Input*, void*);
extern Fluid_Coord_Input *widget_y_input;
extern void w_cb(Fluid_Coord_Input*, void*);
extern Fluid_Coord_Input *widget_w_input;
extern void h_cb(Fluid_Coord_Input*, void*);
extern Fluid_Coord_Input *widget_h_input;
extern void wc_relative_cb(Fl_Choice*, void*);
extern void flex_size_group_cb(Fl_Group*, void*);
#include <FL/Fl_Value_Input.H>
extern void flex_size_cb(Fl_Value_Input*, void*);
extern Fl_Value_Input *widget_flex_size;
#include <FL/Fl_Check_Button.H>
extern void flex_fixed_cb(Fl_Check_Button*, void*);
extern Fl_Check_Button *widget_flex_fixed;
extern void values_group_cb(Fl_Group*, void*);
extern void slider_size_cb(Fl_Value_Input*, void*);
extern void min_cb(Fl_Value_Input*, void*);
extern void max_cb(Fl_Value_Input*, void*);
extern void step_cb(Fl_Value_Input*, void*);
extern void value_cb(Fl_Value_Input*, void*);
extern void flex_margin_group_cb(Fl_Group*, void*);
extern void flex_margin_left_cb(Fl_Value_Input*, void*);
extern void flex_margin_top_cb(Fl_Value_Input*, void*);
extern void flex_margin_right_cb(Fl_Value_Input*, void*);
extern void flex_margin_bottom_cb(Fl_Value_Input*, void*);
extern void flex_margin_gap_cb(Fl_Value_Input*, void*);
extern void size_range_group_cb(Fl_Group*, void*);
extern void min_w_cb(Fl_Value_Input*, void*);
extern void min_h_cb(Fl_Value_Input*, void*);
extern void set_min_size_cb(Fl_Button*, void*);
extern void max_w_cb(Fl_Value_Input*, void*);
extern void max_h_cb(Fl_Value_Input*, void*);
extern void set_max_size_cb(Fl_Button*, void*);
#include <FL/Fl_Shortcut_Button.H>
extern void shortcut_in_cb(Fl_Shortcut_Button*, void*);
extern void xclass_cb(Fl_Input*, void*);
#include <FL/Fl_Light_Button.H>
extern void border_cb(Fl_Light_Button*, void*);
extern void modal_cb(Fl_Light_Button*, void*);
extern void non_modal_cb(Fl_Light_Button*, void*);
extern void visible_cb(Fl_Light_Button*, void*);
extern void active_cb(Fl_Light_Button*, void*);
extern void resizable_cb(Fl_Light_Button*, void*);
extern void hotspot_cb(Fl_Light_Button*, void*);
extern void tooltip_cb(Fl_Input*, void*);
extern Fl_Menu_Item fontmenu[];
extern void labelfont_cb(Fl_Choice*, void*);
extern void labelsize_cb(Fl_Value_Input*, void*);
extern void labelcolor_cb(Fl_Button*, void*);
extern Fl_Button *w_labelcolor;
#include <FL/Fl_Menu_Button.H>
extern Fl_Menu_Item colormenu[];
extern void labelcolor_menu_cb(Fl_Menu_Button*, void*);
extern Fl_Menu_Item boxmenu[];
extern void box_cb(Fl_Choice*, void*);
extern void color_cb(Fl_Button*, void*);
extern Fl_Button *w_color;
extern void color_menu_cb(Fl_Menu_Button*, void*);
extern void down_box_cb(Fl_Choice*, void*);
extern void color2_cb(Fl_Button*, void*);
extern Fl_Button *w_selectcolor;
extern void color2_menu_cb(Fl_Menu_Button*, void*);
extern void textfont_cb(Fl_Choice*, void*);
extern void textsize_cb(Fl_Value_Input*, void*);
extern void textcolor_cb(Fl_Button*, void*);
extern Fl_Button *w_textcolor;
extern void textcolor_menu_cb(Fl_Menu_Button*, void*);
extern void compact_cb(Fl_Light_Button*, void*);
extern void subclass_cb(Fl_Input*, void*);
extern void subtype_cb(Fl_Choice*, void*);
extern void name_cb(Fl_Input*, void*);
extern void name_public_member_cb(Fl_Choice*, void*);
extern void name_public_cb(Fl_Choice*, void*);
extern void v_input_cb(Fl_Input*, void*);
extern Fl_Input *v_input[4];
#include <FL/Fl_Tile.H>
#include <FL/Fl_Text_Editor.H>
extern Fl_Text_Editor *wComment;
#include "CodeEditor.h"
extern void callback_cb(CodeEditor*, void*);
extern CodeEditor *wCallback;
extern void user_data_cb(Fl_Input*, void*);
extern Fl_Menu_Item whenmenu[];
extern void when_cb(Fl_Menu_Button*, void*);
#include <FL/Fl_Input_Choice.H>
extern void user_data_type_cb(Fl_Input_Choice*, void*);
extern Fl_Box *w_when_box;
extern void live_mode_cb(Fl_Button*, void*);
extern Fl_Button *wLiveMode;
extern void overlay_cb(Fl_Button*, void*);
extern Fl_Button *overlay_button;
#include <FL/Fl_Return_Button.H>
extern void ok_cb(Fl_Return_Button*, void*);
Fl_Double_Window* make_widget_panel();
extern Fl_Menu_Item menu_[];
extern Fl_Menu_Item menu_1[];
extern Fl_Menu_Item menu_Children[];
extern Fl_Menu_Item menu_2[];
extern Fl_Menu_Item menu_3[];
extern Fl_Menu_Item menu_4[];
#endif
