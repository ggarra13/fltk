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


File src/drivers/Wayland/Fl_Wayland_Screen_Driver.cxx, added:
monotonic due date for key repetitions.  Fixes key repeats when feedback takes too long.


diff --git a/src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx b/src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx
index b39fbb383..f93d99b16 100644
--- a/src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx
+++ b/src/drivers/Wayland/Fl_Wayland_Window_Driver.cxx
@@ -1082,8 +1082,7 @@ void Fl_Wayland_Window_Driver::wait_for_expose()
   Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
   if (pWindow->fullscreen_active()) {
     if (xid->kind == DECORATED) {
-      while (!(xid->state & LIBDECOR_WINDOW_STATE_FULLSCREEN) ||
-             !(xid->state & LIBDECOR_WINDOW_STATE_ACTIVE)) {
+      while (!(xid->state & LIBDECOR_WINDOW_STATE_FULLSCREEN)) {
         libdecor_dispatch(scr_driver->libdecor_context, 0);
       }
     } else if (xid->kind == UNFRAMED) {
