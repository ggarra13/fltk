File src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx,
function makeWindow, around line 1562:

`````````````````````````````````````````````````````````````````````````````
    checkSubwindowFrame(); // make sure subwindow doesn't leak outside parent
    // \@note: \@bug: NOT on FLTK main branch.  Must keep for fixing bug #1307
    if (can_expand_outside_parent_) parent->covered = true;
  } else { // a window without decoration
`````````````````````````````````````````````````````````````````````````````

File libdecor/src/plugins/gtk/libdecor-gtk.c, added:

#ifndef G_GNUC_FALLTHROUGH
# if defined(__has_attribute)
#  if __has_attribute(fallthrough)
#   define G_GNUC_FALLTHROUGH __attribute__((fallthrough))
#  else
#   define G_GNUC_FALLTHROUGH ((void)0)
#  endif
# else
#  define G_GNUC_FALLTHROUGH ((void)0)
# endif
#endif


