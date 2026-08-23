/// @file sere_qt6.cpp
/// Qt 6 Widgets C ABI for stdlib/qt6.sere. Stubs compile when Qt6 is absent.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef SERE_HAS_QT6
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDate>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDockWidget>
#include <QDial>
#include <QDialog>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QAbstractAnimation>
#include <QBoxLayout>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QFontComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsColorizeEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLCDNumber>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QAbstractSlider>
#include <QSpinBox>
#include <QSplashScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#ifdef _WIN32
#include <windows.h>
#endif
#endif

extern "C" {

static void outEmpty(const char** out_data, int64_t* out_len) {
  if (out_data != nullptr) {
    *out_data = "";
  }
  if (out_len != nullptr) {
    *out_len = 0;
  }
}

#ifndef SERE_HAS_QT6

int32_t sere_qt6_available(void) { return 0; }
void* sere_qt6_app_new(void) { return nullptr; }
int32_t sere_qt6_app_exec(void* app) {
  (void)app;
  return 1;
}
void sere_qt6_app_process(void* app) { (void)app; }
void sere_qt6_app_quit(void* app) { (void)app; }
void sere_qt6_app_set_name(void* app, const char* text, int64_t len) {
  (void)app;
  (void)text;
  (void)len;
}
void sere_qt6_app_set_org(void* app, const char* text, int64_t len) {
  (void)app;
  (void)text;
  (void)len;
}
void sere_qt6_app_set_style(void* app, const char* text, int64_t len) {
  (void)app;
  (void)text;
  (void)len;
}
void sere_qt6_app_set_stylesheet(void* app, const char* text, int64_t len) {
  (void)app;
  (void)text;
  (void)len;
}
void sere_qt6_app_stylesheet(void* app, const char** out_data, int64_t* out_len) {
  (void)app;
  outEmpty(out_data, out_len);
}
void sere_qt6_app_set_palette_dark(void* app) { (void)app; }
void sere_qt6_app_set_palette_light(void* app) { (void)app; }
void sere_qt6_app_set_font(void* app, const char* family, int64_t family_len, int32_t size,
                           int32_t weight, int32_t italic) {
  (void)app;
  (void)family;
  (void)family_len;
  (void)size;
  (void)weight;
  (void)italic;
}
void sere_qt6_styles(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
void* sere_qt6_clipboard(void) { return nullptr; }
void sere_qt6_clipboard_text(const char** out_data, int64_t* out_len) { outEmpty(out_data, out_len); }
void sere_qt6_clipboard_set(const char* text, int64_t len) {
  (void)text;
  (void)len;
}
void sere_qt6_open_url(const char* url, int64_t len) {
  (void)url;
  (void)len;
}
int32_t sere_qt6_screen_width(void) { return 0; }
int32_t sere_qt6_screen_height(void) { return 0; }
int32_t sere_qt6_screen_dpi(void) { return 96; }
void* sere_qt6_widget_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_window_new(const char* title, int64_t title_len) {
  (void)title;
  (void)title_len;
  return nullptr;
}
void* sere_qt6_dialog_new(void* parent, const char* title, int64_t title_len) {
  (void)parent;
  (void)title;
  (void)title_len;
  return nullptr;
}
int32_t sere_qt6_dialog_exec(void* dialog) {
  (void)dialog;
  return 0;
}
void sere_qt6_dialog_accept(void* dialog) { (void)dialog; }
void sere_qt6_dialog_reject(void* dialog) { (void)dialog; }
void* sere_qt6_window_central(void* window) {
  (void)window;
  return nullptr;
}
void sere_qt6_window_set_central(void* window, void* widget) {
  (void)window;
  (void)widget;
}
void* sere_qt6_menu_bar(void* window) {
  (void)window;
  return nullptr;
}
void* sere_qt6_status_bar(void* window) {
  (void)window;
  return nullptr;
}
void sere_qt6_status_message(void* bar, const char* text, int64_t len, int32_t ms) {
  (void)bar;
  (void)text;
  (void)len;
  (void)ms;
}
void* sere_qt6_tool_bar(void* window, const char* title, int64_t title_len) {
  (void)window;
  (void)title;
  (void)title_len;
  return nullptr;
}
void sere_qt6_add_dock(void* window, void* widget, int32_t area) {
  (void)window;
  (void)widget;
  (void)area;
}
void sere_qt6_widget_set_title(void* widget, const char* title, int64_t title_len) {
  (void)widget;
  (void)title;
  (void)title_len;
}
void sere_qt6_widget_title(void* widget, const char** out_data, int64_t* out_len) {
  (void)widget;
  outEmpty(out_data, out_len);
}
void sere_qt6_widget_resize(void* widget, int32_t width, int32_t height) {
  (void)widget;
  (void)width;
  (void)height;
}
void sere_qt6_widget_move(void* widget, int32_t x, int32_t y) {
  (void)widget;
  (void)x;
  (void)y;
}
int32_t sere_qt6_widget_x(void* widget) {
  (void)widget;
  return 0;
}
int32_t sere_qt6_widget_y(void* widget) {
  (void)widget;
  return 0;
}
int32_t sere_qt6_widget_width(void* widget) {
  (void)widget;
  return 0;
}
int32_t sere_qt6_widget_height(void* widget) {
  (void)widget;
  return 0;
}
void sere_qt6_widget_show(void* widget) { (void)widget; }
void sere_qt6_widget_hide(void* widget) { (void)widget; }
void sere_qt6_widget_close(void* widget) { (void)widget; }
void sere_qt6_widget_raise(void* widget) { (void)widget; }
void sere_qt6_widget_lower(void* widget) { (void)widget; }
void sere_qt6_widget_update(void* widget) { (void)widget; }
void sere_qt6_widget_focus(void* widget) { (void)widget; }
void sere_qt6_widget_set_enabled(void* widget, int32_t enabled) {
  (void)widget;
  (void)enabled;
}
void sere_qt6_widget_set_visible(void* widget, int32_t visible) {
  (void)widget;
  (void)visible;
}
void sere_qt6_widget_set_stylesheet(void* widget, const char* text, int64_t len) {
  (void)widget;
  (void)text;
  (void)len;
}
void sere_qt6_widget_stylesheet(void* widget, const char** out_data, int64_t* out_len) {
  (void)widget;
  outEmpty(out_data, out_len);
}
void sere_qt6_widget_set_object_name(void* widget, const char* text, int64_t len) {
  (void)widget;
  (void)text;
  (void)len;
}
void sere_qt6_widget_set_property(void* widget, const char* key, int64_t key_len, const char* val,
                                  int64_t val_len) {
  (void)widget;
  (void)key;
  (void)key_len;
  (void)val;
  (void)val_len;
}
void sere_qt6_widget_set_tooltip(void* widget, const char* text, int64_t len) {
  (void)widget;
  (void)text;
  (void)len;
}
void sere_qt6_widget_set_cursor(void* widget, int32_t shape) {
  (void)widget;
  (void)shape;
}
void sere_qt6_widget_set_font(void* widget, const char* family, int64_t family_len, int32_t size,
                              int32_t weight, int32_t italic) {
  (void)widget;
  (void)family;
  (void)family_len;
  (void)size;
  (void)weight;
  (void)italic;
}
void sere_qt6_widget_set_palette_color(void* widget, int32_t role, int32_t r, int32_t g, int32_t b,
                                       int32_t a) {
  (void)widget;
  (void)role;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_qt6_widget_set_min_size(void* widget, int32_t width, int32_t height) {
  (void)widget;
  (void)width;
  (void)height;
}
void sere_qt6_widget_set_max_size(void* widget, int32_t width, int32_t height) {
  (void)widget;
  (void)width;
  (void)height;
}
void sere_qt6_widget_set_fixed_size(void* widget, int32_t width, int32_t height) {
  (void)widget;
  (void)width;
  (void)height;
}
void sere_qt6_widget_set_margins(void* widget, int32_t l, int32_t t, int32_t r, int32_t b) {
  (void)widget;
  (void)l;
  (void)t;
  (void)r;
  (void)b;
}
void sere_qt6_widget_set_size_policy(void* widget, int32_t h, int32_t v) {
  (void)widget;
  (void)h;
  (void)v;
}
void sere_qt6_widget_set_layout(void* widget, void* layout) {
  (void)widget;
  (void)layout;
}
void sere_qt6_widget_set_parent(void* widget, void* parent) {
  (void)widget;
  (void)parent;
}
void sere_qt6_widget_set_attr(void* widget, int32_t attr, int32_t on) {
  (void)widget;
  (void)attr;
  (void)on;
}
void sere_qt6_widget_set_flags(void* widget, int32_t flags) {
  (void)widget;
  (void)flags;
}
void sere_qt6_widget_set_modality(void* widget, int32_t modality) {
  (void)widget;
  (void)modality;
}
void sere_qt6_widget_set_icon(void* widget, const char* path, int64_t path_len) {
  (void)widget;
  (void)path;
  (void)path_len;
}
void sere_qt6_widget_set_window_icon(void* widget, const char* path, int64_t path_len) {
  (void)widget;
  (void)path;
  (void)path_len;
}
int32_t sere_qt6_take(void* obj, int32_t kind) {
  (void)obj;
  (void)kind;
  return 0;
}
int32_t sere_qt6_last_int(void* obj) {
  (void)obj;
  return 0;
}
void sere_qt6_last_text(void* obj, const char** out_data, int64_t* out_len) {
  (void)obj;
  outEmpty(out_data, out_len);
}
void* sere_qt6_label_new(void* parent, const char* text, int64_t text_len) {
  (void)parent;
  (void)text;
  (void)text_len;
  return nullptr;
}
void sere_qt6_label_set_text(void* label, const char* text, int64_t text_len) {
  (void)label;
  (void)text;
  (void)text_len;
}
void sere_qt6_label_set_align(void* label, int32_t align) {
  (void)label;
  (void)align;
}
void sere_qt6_label_set_wrap(void* label, int32_t wrap) {
  (void)label;
  (void)wrap;
}
void sere_qt6_label_set_pixmap(void* label, const char* path, int64_t path_len) {
  (void)label;
  (void)path;
  (void)path_len;
}
void* sere_qt6_button_new(void* parent, const char* text, int64_t text_len, int32_t kind) {
  (void)parent;
  (void)text;
  (void)text_len;
  (void)kind;
  return nullptr;
}
void sere_qt6_button_set_text(void* button, const char* text, int64_t text_len) {
  (void)button;
  (void)text;
  (void)text_len;
}
void sere_qt6_button_set_checked(void* button, int32_t checked) {
  (void)button;
  (void)checked;
}
int32_t sere_qt6_button_checked(void* button) {
  (void)button;
  return 0;
}
void sere_qt6_button_set_checkable(void* button, int32_t on) {
  (void)button;
  (void)on;
}
void* sere_qt6_line_new(void* parent, const char* text, int64_t text_len) {
  (void)parent;
  (void)text;
  (void)text_len;
  return nullptr;
}
void sere_qt6_line_set_text(void* line, const char* text, int64_t text_len) {
  (void)line;
  (void)text;
  (void)text_len;
}
void sere_qt6_line_text(void* line, const char** out_data, int64_t* out_len) {
  (void)line;
  outEmpty(out_data, out_len);
}
void sere_qt6_line_set_placeholder(void* line, const char* text, int64_t text_len) {
  (void)line;
  (void)text;
  (void)text_len;
}
void sere_qt6_line_set_echo(void* line, int32_t mode) {
  (void)line;
  (void)mode;
}
void sere_qt6_line_set_read_only(void* line, int32_t on) {
  (void)line;
  (void)on;
}
void* sere_qt6_text_new(void* parent, const char* text, int64_t text_len, int32_t plain) {
  (void)parent;
  (void)text;
  (void)text_len;
  (void)plain;
  return nullptr;
}
void sere_qt6_text_set(void* edit, const char* text, int64_t text_len) {
  (void)edit;
  (void)text;
  (void)text_len;
}
void sere_qt6_text_get(void* edit, const char** out_data, int64_t* out_len) {
  (void)edit;
  outEmpty(out_data, out_len);
}
void* sere_qt6_spin_new(void* parent, int32_t is_double) {
  (void)parent;
  (void)is_double;
  return nullptr;
}
void sere_qt6_spin_set_range(void* spin, double lo, double hi) {
  (void)spin;
  (void)lo;
  (void)hi;
}
void sere_qt6_spin_set_value(void* spin, double value) {
  (void)spin;
  (void)value;
}
double sere_qt6_spin_value(void* spin) {
  (void)spin;
  return 0.0;
}
void* sere_qt6_slider_new(void* parent, int32_t orient, int32_t kind) {
  (void)parent;
  (void)orient;
  (void)kind;
  return nullptr;
}
void sere_qt6_slider_set_range(void* slider, int32_t lo, int32_t hi) {
  (void)slider;
  (void)lo;
  (void)hi;
}
void sere_qt6_slider_set_value(void* slider, int32_t value) {
  (void)slider;
  (void)value;
}
int32_t sere_qt6_slider_value(void* slider) {
  (void)slider;
  return 0;
}
void* sere_qt6_progress_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_progress_set(void* bar, int32_t value) {
  (void)bar;
  (void)value;
}
void sere_qt6_progress_set_range(void* bar, int32_t lo, int32_t hi) {
  (void)bar;
  (void)lo;
  (void)hi;
}
void* sere_qt6_combo_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_combo_add(void* combo, const char* text, int64_t text_len) {
  (void)combo;
  (void)text;
  (void)text_len;
}
void sere_qt6_combo_clear(void* combo) { (void)combo; }
int32_t sere_qt6_combo_index(void* combo) {
  (void)combo;
  return -1;
}
void sere_qt6_combo_set_index(void* combo, int32_t index) {
  (void)combo;
  (void)index;
}
void sere_qt6_combo_text(void* combo, const char** out_data, int64_t* out_len) {
  (void)combo;
  outEmpty(out_data, out_len);
}
void* sere_qt6_list_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_list_add(void* list, const char* text, int64_t text_len) {
  (void)list;
  (void)text;
  (void)text_len;
}
void sere_qt6_list_clear(void* list) { (void)list; }
int32_t sere_qt6_list_row(void* list) {
  (void)list;
  return -1;
}
void sere_qt6_list_item(void* list, int32_t row, const char** out_data, int64_t* out_len) {
  (void)list;
  (void)row;
  outEmpty(out_data, out_len);
}
void* sere_qt6_table_new(void* parent, int32_t rows, int32_t cols) {
  (void)parent;
  (void)rows;
  (void)cols;
  return nullptr;
}
void sere_qt6_table_set(void* table, int32_t row, int32_t col, const char* text, int64_t text_len) {
  (void)table;
  (void)row;
  (void)col;
  (void)text;
  (void)text_len;
}
void sere_qt6_table_header(void* table, int32_t col, const char* text, int64_t text_len) {
  (void)table;
  (void)col;
  (void)text;
  (void)text_len;
}
void sere_qt6_table_set_rows(void* table, int32_t rows) {
  (void)table;
  (void)rows;
}
int32_t sere_qt6_table_row(void* table) {
  (void)table;
  return -1;
}
int32_t sere_qt6_table_col(void* table) {
  (void)table;
  return -1;
}
void* sere_qt6_tree_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_tree_add(void* tree, const char* text, int64_t text_len) {
  (void)tree;
  (void)text;
  (void)text_len;
}
void* sere_qt6_tabs_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_tabs_add(void* tabs, void* page, const char* title, int64_t title_len) {
  (void)tabs;
  (void)page;
  (void)title;
  (void)title_len;
}
int32_t sere_qt6_tabs_index(void* tabs) {
  (void)tabs;
  return 0;
}
void sere_qt6_tabs_set_index(void* tabs, int32_t index) {
  (void)tabs;
  (void)index;
}
void* sere_qt6_group_new(void* parent, const char* title, int64_t title_len) {
  (void)parent;
  (void)title;
  (void)title_len;
  return nullptr;
}
void* sere_qt6_frame_new(void* parent, int32_t shape) {
  (void)parent;
  (void)shape;
  return nullptr;
}
void* sere_qt6_scroll_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_scroll_set(void* scroll, void* widget) {
  (void)scroll;
  (void)widget;
}
void* sere_qt6_splitter_new(void* parent, int32_t orient) {
  (void)parent;
  (void)orient;
  return nullptr;
}
void sere_qt6_splitter_add(void* splitter, void* widget) {
  (void)splitter;
  (void)widget;
}
void sere_qt6_splitter_sizes(void* splitter, int32_t a, int32_t b) {
  (void)splitter;
  (void)a;
  (void)b;
}
void* sere_qt6_stack_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_stack_add(void* stack, void* widget) {
  (void)stack;
  (void)widget;
}
void sere_qt6_stack_set(void* stack, int32_t index) {
  (void)stack;
  (void)index;
}
void* sere_qt6_lcd_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_lcd_set(void* lcd, int32_t value) {
  (void)lcd;
  (void)value;
}
void* sere_qt6_calendar_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_date_new(void* parent, int32_t time_edit) {
  (void)parent;
  (void)time_edit;
  return nullptr;
}
void* sere_qt6_vbox_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_hbox_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_grid_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_form_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_layout_add(void* layout, void* widget, int32_t stretch) {
  (void)layout;
  (void)widget;
  (void)stretch;
}
void sere_qt6_layout_add_stretch(void* layout, int32_t stretch) {
  (void)layout;
  (void)stretch;
}
void sere_qt6_layout_spacing(void* layout, int32_t spacing) {
  (void)layout;
  (void)spacing;
}
void sere_qt6_layout_margins(void* layout, int32_t l, int32_t t, int32_t r, int32_t b) {
  (void)layout;
  (void)l;
  (void)t;
  (void)r;
  (void)b;
}
void sere_qt6_grid_add(void* layout, void* widget, int32_t row, int32_t col, int32_t rs,
                       int32_t cs) {
  (void)layout;
  (void)widget;
  (void)row;
  (void)col;
  (void)rs;
  (void)cs;
}
void sere_qt6_form_add(void* layout, const char* label, int64_t label_len, void* widget) {
  (void)layout;
  (void)label;
  (void)label_len;
  (void)widget;
}
void* sere_qt6_menu_add(void* bar, const char* title, int64_t title_len) {
  (void)bar;
  (void)title;
  (void)title_len;
  return nullptr;
}
void* sere_qt6_menu_action(void* menu, const char* text, int64_t text_len, const char* shortcut,
                           int64_t shortcut_len) {
  (void)menu;
  (void)text;
  (void)text_len;
  (void)shortcut;
  (void)shortcut_len;
  return nullptr;
}
void sere_qt6_menu_separator(void* menu) { (void)menu; }
void sere_qt6_toolbar_add(void* bar, void* action) {
  (void)bar;
  (void)action;
}
void* sere_qt6_timer_new(int32_t ms) {
  (void)ms;
  return nullptr;
}
void sere_qt6_timer_start(void* timer) { (void)timer; }
void sere_qt6_timer_stop(void* timer) { (void)timer; }
void sere_qt6_timer_set_interval(void* timer, int32_t ms) {
  (void)timer;
  (void)ms;
}
void* sere_qt6_shortcut_new(void* parent, const char* seq, int64_t seq_len) {
  (void)parent;
  (void)seq;
  (void)seq_len;
  return nullptr;
}
void sere_qt6_file_open(void* parent, const char* caption, int64_t cap_len, const char* filter,
                        int64_t filter_len, const char** out_data, int64_t* out_len) {
  (void)parent;
  (void)caption;
  (void)cap_len;
  (void)filter;
  (void)filter_len;
  outEmpty(out_data, out_len);
}
void sere_qt6_file_save(void* parent, const char* caption, int64_t cap_len, const char* filter,
                        int64_t filter_len, const char** out_data, int64_t* out_len) {
  (void)parent;
  (void)caption;
  (void)cap_len;
  (void)filter;
  (void)filter_len;
  outEmpty(out_data, out_len);
}
void sere_qt6_dir_open(void* parent, const char* caption, int64_t cap_len, const char** out_data,
                       int64_t* out_len) {
  (void)parent;
  (void)caption;
  (void)cap_len;
  outEmpty(out_data, out_len);
}
int32_t sere_qt6_color_dialog(void* parent, int32_t r, int32_t g, int32_t b, int32_t* out_r,
                              int32_t* out_g, int32_t* out_b) {
  (void)parent;
  (void)r;
  (void)g;
  (void)b;
  if (out_r != nullptr) {
    *out_r = r;
  }
  if (out_g != nullptr) {
    *out_g = g;
  }
  if (out_b != nullptr) {
    *out_b = b;
  }
  return 0;
}
void sere_qt6_font_dialog(void* parent, const char** out_data, int64_t* out_len) {
  (void)parent;
  outEmpty(out_data, out_len);
}
int32_t sere_qt6_message(void* parent, int32_t kind, const char* title, int64_t title_len,
                         const char* text, int64_t text_len) {
  (void)parent;
  (void)kind;
  (void)title;
  (void)title_len;
  (void)text;
  (void)text_len;
  return 0;
}
void sere_qt6_input_text(void* parent, const char* title, int64_t title_len, const char* label,
                         int64_t label_len, const char** out_data, int64_t* out_len) {
  (void)parent;
  (void)title;
  (void)title_len;
  (void)label;
  (void)label_len;
  outEmpty(out_data, out_len);
}
void sere_qt6_settings_set(const char* key, int64_t key_len, const char* val, int64_t val_len) {
  (void)key;
  (void)key_len;
  (void)val;
  (void)val_len;
}
void sere_qt6_settings_get(const char* key, int64_t key_len, const char** out_data,
                           int64_t* out_len) {
  (void)key;
  (void)key_len;
  outEmpty(out_data, out_len);
}
void* sere_qt6_tray_new(const char* tip, int64_t tip_len) {
  (void)tip;
  (void)tip_len;
  return nullptr;
}
void sere_qt6_tray_show(void* tray, const char* title, int64_t title_len, const char* msg,
                        int64_t msg_len) {
  (void)tray;
  (void)title;
  (void)title_len;
  (void)msg;
  (void)msg_len;
}

void sere_qt6_widget_opacity(void* widget, double value) {
  (void)widget;
  (void)value;
}
int32_t sere_qt6_widget_enabled(void* widget) {
  (void)widget;
  return 0;
}
int32_t sere_qt6_widget_visible(void* widget) {
  (void)widget;
  return 0;
}
void sere_qt6_widget_set_state(void* widget, int32_t state) {
  (void)widget;
  (void)state;
}
int32_t sere_qt6_widget_state(void* widget) {
  (void)widget;
  return 0;
}
void sere_qt6_widget_center(void* widget) { (void)widget; }
void sere_qt6_widget_adjust(void* widget) { (void)widget; }
void sere_qt6_widget_activate(void* widget) { (void)widget; }
void sere_qt6_widget_set_status_tip(void* widget, const char* text, int64_t len) {
  (void)widget;
  (void)text;
  (void)len;
}
void sere_qt6_widget_set_whats_this(void* widget, const char* text, int64_t len) {
  (void)widget;
  (void)text;
  (void)len;
}
void sere_qt6_widget_set_focus_policy(void* widget, int32_t policy) {
  (void)widget;
  (void)policy;
}
void sere_qt6_widget_set_context_policy(void* widget, int32_t policy) {
  (void)widget;
  (void)policy;
}
void sere_qt6_widget_set_accept_drops(void* widget, int32_t on) {
  (void)widget;
  (void)on;
}
void sere_qt6_widget_set_mouse_tracking(void* widget, int32_t on) {
  (void)widget;
  (void)on;
}
void sere_qt6_effect_opacity(void* widget, double value) {
  (void)widget;
  (void)value;
}
void sere_qt6_effect_blur(void* widget, double radius) {
  (void)widget;
  (void)radius;
}
void sere_qt6_effect_shadow(void* widget, int32_t dx, int32_t dy, int32_t blur, int32_t r, int32_t g,
                            int32_t b, int32_t a) {
  (void)widget;
  (void)dx;
  (void)dy;
  (void)blur;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}
void sere_qt6_effect_colorize(void* widget, int32_t r, int32_t g, int32_t b, double strength) {
  (void)widget;
  (void)r;
  (void)g;
  (void)b;
  (void)strength;
}
void sere_qt6_effect_clear(void* widget) { (void)widget; }
void sere_qt6_anim_opacity(void* widget, double from, double to, int32_t ms) {
  (void)widget;
  (void)from;
  (void)to;
  (void)ms;
}
void sere_qt6_anim_move(void* widget, int32_t x, int32_t y, int32_t w, int32_t h, int32_t ms) {
  (void)widget;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)ms;
}
void sere_qt6_text_set_html(void* edit, const char* text, int64_t len) {
  (void)edit;
  (void)text;
  (void)len;
}
void sere_qt6_text_html(void* edit, const char** out_data, int64_t* out_len) {
  (void)edit;
  outEmpty(out_data, out_len);
}
void sere_qt6_text_set_markdown(void* edit, const char* text, int64_t len) {
  (void)edit;
  (void)text;
  (void)len;
}
void sere_qt6_text_append(void* edit, const char* text, int64_t len) {
  (void)edit;
  (void)text;
  (void)len;
}
void sere_qt6_label_open_links(void* label, int32_t on) {
  (void)label;
  (void)on;
}
void sere_qt6_line_max_len(void* line, int32_t n) {
  (void)line;
  (void)n;
}
void sere_qt6_line_select_all(void* line) { (void)line; }
void sere_qt6_line_clear(void* line) { (void)line; }
void sere_qt6_line_completer(void* line, const char* items, int64_t len) {
  (void)line;
  (void)items;
  (void)len;
}
void sere_qt6_combo_editable(void* combo, int32_t on) {
  (void)combo;
  (void)on;
}
int32_t sere_qt6_combo_count(void* combo) {
  (void)combo;
  return 0;
}
int32_t sere_qt6_list_count(void* list) {
  (void)list;
  return 0;
}
void sere_qt6_list_set_row(void* list, int32_t row) {
  (void)list;
  (void)row;
}
void sere_qt6_table_get(void* table, int32_t row, int32_t col, const char** out_data,
                        int64_t* out_len) {
  (void)table;
  (void)row;
  (void)col;
  outEmpty(out_data, out_len);
}
void sere_qt6_table_cols(void* table, int32_t cols) {
  (void)table;
  (void)cols;
}
void sere_qt6_table_stretch(void* table, int32_t on) {
  (void)table;
  (void)on;
}
void sere_qt6_tree_clear(void* tree) { (void)tree; }
void sere_qt6_tabs_pos(void* tabs, int32_t pos) {
  (void)tabs;
  (void)pos;
}
void sere_qt6_tabs_closable(void* tabs, int32_t on) {
  (void)tabs;
  (void)on;
}
void sere_qt6_tabs_movable(void* tabs, int32_t on) {
  (void)tabs;
  (void)on;
}
void sere_qt6_tabs_document(void* tabs, int32_t on) {
  (void)tabs;
  (void)on;
}
void sere_qt6_scroll_resizable(void* scroll, int32_t on) {
  (void)scroll;
  (void)on;
}
void sere_qt6_progress_text(void* bar, int32_t on) {
  (void)bar;
  (void)on;
}
void sere_qt6_spin_suffix(void* spin, const char* text, int64_t len) {
  (void)spin;
  (void)text;
  (void)len;
}
void sere_qt6_slider_ticks(void* slider, int32_t pos, int32_t interval) {
  (void)slider;
  (void)pos;
  (void)interval;
}
void sere_qt6_group_checkable(void* group, int32_t on) {
  (void)group;
  (void)on;
}
void sere_qt6_window_dock_nesting(void* window, int32_t on) {
  (void)window;
  (void)on;
}
int32_t sere_qt6_color_pick(void* parent, int32_t r, int32_t g, int32_t b) {
  (void)parent;
  (void)r;
  (void)g;
  (void)b;
  return -1;
}
void* sere_qt6_vbox_bare(void) { return nullptr; }
void* sere_qt6_hbox_bare(void) { return nullptr; }
void sere_qt6_layout_add_layout(void* layout, void* child, int32_t stretch) {
  (void)layout;
  (void)child;
  (void)stretch;
}
void* sere_qt6_browser_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_toolbox_new(void* parent) {
  (void)parent;
  return nullptr;
}
void sere_qt6_toolbox_add(void* box, void* page, const char* title, int64_t len) {
  (void)box;
  (void)page;
  (void)title;
  (void)len;
}
void* sere_qt6_mdi_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_mdi_add(void* mdi, void* widget, const char* title, int64_t len) {
  (void)mdi;
  (void)widget;
  (void)title;
  (void)len;
  return nullptr;
}
void* sere_qt6_splash_new(const char* text, int64_t len) {
  (void)text;
  (void)len;
  return nullptr;
}
void sere_qt6_splash_finish(void* splash, void* window) {
  (void)splash;
  (void)window;
}
void* sere_qt6_fontcombo_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_buttonbox_new(void* parent) {
  (void)parent;
  return nullptr;
}
void* sere_qt6_buttonbox_add(void* box, const char* text, int64_t len, int32_t role) {
  (void)box;
  (void)text;
  (void)len;
  (void)role;
  return nullptr;
}
void sere_qt6_buttonbox_std(void* box, int32_t buttons) {
  (void)box;
  (void)buttons;
}
void sere_qt6_menu_popup(void* menu, int32_t x, int32_t y) {
  (void)menu;
  (void)x;
  (void)y;
}
void sere_qt6_action_set_checkable(void* action, int32_t on) {
  (void)action;
  (void)on;
}
int32_t sere_qt6_action_checked(void* action) {
  (void)action;
  return 0;
}
void sere_qt6_action_set_enabled(void* action, int32_t on) {
  (void)action;
  (void)on;
}
void sere_qt6_action_set_text(void* action, const char* text, int64_t len) {
  (void)action;
  (void)text;
  (void)len;
}
void sere_qt6_action_set_shortcut(void* action, const char* seq, int64_t len) {
  (void)action;
  (void)seq;
  (void)len;
}
void sere_qt6_app_set_palette_accent(void* app, int32_t r, int32_t g, int32_t b) {
  (void)app;
  (void)r;
  (void)g;
  (void)b;
}
void sere_qt6_app_beep(void) {}
void sere_qt6_app_about_qt(void* parent) { (void)parent; }
void sere_qt6_calendar_iso(void* cal, const char** out_data, int64_t* out_len) {
  (void)cal;
  outEmpty(out_data, out_len);
}
void sere_qt6_date_iso(void* edit, const char** out_data, int64_t* out_len) {
  (void)edit;
  outEmpty(out_data, out_len);
}
void sere_qt6_date_set_iso(void* edit, const char* text, int64_t len) {
  (void)edit;
  (void)text;
  (void)len;
}

#else

static void outOwned(const std::string& text, const char** out_data, int64_t* out_len) {
  char* copy = static_cast<char*>(std::malloc(text.size() + 1));
  if (copy == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  std::memcpy(copy, text.data(), text.size());
  copy[text.size()] = '\0';
  if (out_data != nullptr) {
    *out_data = copy;
  }
  if (out_len != nullptr) {
    *out_len = static_cast<int64_t>(text.size());
  }
}

namespace {

int g_argc = 1;
char g_arg0[] = "sere";
char* g_argv[] = {g_arg0, nullptr};

struct EventBox {
  int32_t flags[16]{};
  int32_t iValue = 0;
  int32_t iValue2 = 0;
  std::string text;
};

std::unordered_map<void*, EventBox> g_events;

QString fromSere(const char* data, int64_t len) {
  if (data == nullptr || len <= 0) {
    return QString();
  }
  return QString::fromUtf8(data, static_cast<int>(len));
}

QWidget* asW(void* pointer) { return static_cast<QWidget*>(pointer); }

QWidget* layoutHost(void* parent) {
  QWidget* widget = asW(parent);
  if (auto* main = qobject_cast<QMainWindow*>(widget)) {
    QWidget* central = main->centralWidget();
    if (central == nullptr) {
      central = new QWidget(main);
      main->setCentralWidget(central);
    }
    return central;
  }
  return widget;
}

EventBox& box(void* pointer) { return g_events[pointer]; }

void ping(void* pointer, int32_t kind, int32_t value = 0, const QString& text = QString()) {
  EventBox& events = box(pointer);
  if (kind >= 1 && kind < 16) {
    events.flags[kind] = 1;
  }
  events.iValue = value;
  events.text = text.toUtf8().toStdString();
}

void connectButton(QAbstractButton* button) {
  QObject::connect(button, &QAbstractButton::clicked, [button]() { ping(button, 1); });
  QObject::connect(button, &QAbstractButton::toggled, [button](bool on) {
    ping(button, 2, on ? 1 : 0);
  });
}

void applyFont(QWidget* widget, const char* family, int64_t family_len, int32_t size,
               int32_t weight, int32_t italic) {
  if (widget == nullptr) {
    return;
  }
  QFont font = widget->font();
  if (family_len > 0) {
    font.setFamily(fromSere(family, family_len));
  }
  if (size > 0) {
    font.setPointSize(size);
  }
  if (weight > 0) {
    font.setWeight(static_cast<QFont::Weight>(weight));
  }
  font.setItalic(italic != 0);
  widget->setFont(font);
}

QPalette darkPalette() {
  QPalette palette;
  const QColor window(32, 32, 36);
  const QColor base(24, 24, 28);
  const QColor alt(40, 40, 46);
  const QColor text(230, 230, 234);
  const QColor disabled(140, 140, 148);
  const QColor button(48, 48, 54);
  const QColor highlight(88, 101, 242);
  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::AlternateBase, alt);
  palette.setColor(QPalette::ToolTipBase, alt);
  palette.setColor(QPalette::ToolTipText, text);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::Button, button);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::BrightText, QColor(255, 80, 80));
  palette.setColor(QPalette::Highlight, highlight);
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::PlaceholderText, disabled);
  palette.setColor(QPalette::Link, QColor(120, 170, 255));
  palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
  return palette;
}

void ensureQtPaths() {
#ifdef SERE_QT6_BIN
#ifdef _WIN32
  SetDllDirectoryA(SERE_QT6_BIN);
#endif
#endif
#ifdef SERE_QT6_PLUGINS
  QCoreApplication::addLibraryPath(QString::fromUtf8(SERE_QT6_PLUGINS));
#endif
}

}  // namespace

int32_t sere_qt6_available(void) { return 1; }

namespace {
const unsigned char kSereIconIco[] = {
#include "sere_icon_ico.inc"
};

QIcon sereDefaultWindowIcon() {
  static QIcon icon;
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    QPixmap pix;
    if (pix.loadFromData(kSereIconIco, static_cast<uint>(sizeof(kSereIconIco)), "ICO")) {
      icon = QIcon(pix);
    }
  }
  return icon;
}

void applySereDefaultIcon(QWidget* widget) {
  const QIcon icon = sereDefaultWindowIcon();
  if (widget != nullptr && !icon.isNull()) {
    widget->setWindowIcon(icon);
  }
}
}  // namespace

void* sere_qt6_app_new(void) {
  ensureQtPaths();
  if (QApplication::instance() != nullptr) {
    return QApplication::instance();
  }
  auto* app = new QApplication(g_argc, g_argv);
  app->setStyle("Fusion");
  const QIcon icon = sereDefaultWindowIcon();
  if (!icon.isNull()) {
    app->setWindowIcon(icon);
  }
  return app;
}

int32_t sere_qt6_app_exec(void* app) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  return qt == nullptr ? 1 : qt->exec();
}

void sere_qt6_app_process(void* app) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->processEvents();
  }
}

void sere_qt6_app_quit(void* app) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->quit();
  }
}

void sere_qt6_app_set_name(void* app, const char* text, int64_t len) {
  (void)app;
  QCoreApplication::setApplicationName(fromSere(text, len));
}

void sere_qt6_app_set_org(void* app, const char* text, int64_t len) {
  (void)app;
  QCoreApplication::setOrganizationName(fromSere(text, len));
}

void sere_qt6_app_set_style(void* app, const char* text, int64_t len) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->setStyle(fromSere(text, len));
  }
}

void sere_qt6_app_set_stylesheet(void* app, const char* text, int64_t len) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->setStyleSheet(fromSere(text, len));
  }
}

void sere_qt6_app_stylesheet(void* app, const char** out_data, int64_t* out_len) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->styleSheet().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_app_set_palette_dark(void* app) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->setStyle("Fusion");
    qt->setPalette(darkPalette());
  }
}

void sere_qt6_app_set_palette_light(void* app) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt != nullptr) {
    qt->setPalette(qt->style()->standardPalette());
  }
}

void sere_qt6_app_set_font(void* app, const char* family, int64_t family_len, int32_t size,
                           int32_t weight, int32_t italic) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt == nullptr) {
    return;
  }
  QFont font = qt->font();
  if (family_len > 0) {
    font.setFamily(fromSere(family, family_len));
  }
  if (size > 0) {
    font.setPointSize(size);
  }
  if (weight > 0) {
    font.setWeight(static_cast<QFont::Weight>(weight));
  }
  font.setItalic(italic != 0);
  qt->setFont(font);
}

void sere_qt6_styles(const char** out_data, int64_t* out_len) {
  outOwned(QStyleFactory::keys().join('\n').toUtf8().toStdString(), out_data, out_len);
}

void* sere_qt6_clipboard(void) {
  return QApplication::clipboard();
}

void sere_qt6_clipboard_text(const char** out_data, int64_t* out_len) {
  QClipboard* clip = QApplication::clipboard();
  if (clip == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(clip->text().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_clipboard_set(const char* text, int64_t len) {
  QClipboard* clip = QApplication::clipboard();
  if (clip != nullptr) {
    clip->setText(fromSere(text, len));
  }
}

void sere_qt6_open_url(const char* url, int64_t len) {
  QDesktopServices::openUrl(QUrl(fromSere(url, len)));
}

int32_t sere_qt6_screen_width(void) {
  QScreen* screen = QGuiApplication::primaryScreen();
  return screen == nullptr ? 0 : screen->geometry().width();
}

int32_t sere_qt6_screen_height(void) {
  QScreen* screen = QGuiApplication::primaryScreen();
  return screen == nullptr ? 0 : screen->geometry().height();
}

int32_t sere_qt6_screen_dpi(void) {
  QScreen* screen = QGuiApplication::primaryScreen();
  return screen == nullptr ? 96 : static_cast<int32_t>(screen->logicalDotsPerInch());
}

void* sere_qt6_widget_new(void* parent) { return new QWidget(asW(parent)); }

void* sere_qt6_window_new(const char* title, int64_t title_len) {
  auto* window = new QMainWindow();
  window->setWindowTitle(fromSere(title, title_len));
  window->resize(960, 640);
  auto* central = new QWidget(window);
  window->setCentralWidget(central);
  window->statusBar();
  applySereDefaultIcon(window);
  return window;
}

void* sere_qt6_dialog_new(void* parent, const char* title, int64_t title_len) {
  auto* dialog = new QDialog(asW(parent));
  dialog->setWindowTitle(fromSere(title, title_len));
  applySereDefaultIcon(dialog);
  return dialog;
}

int32_t sere_qt6_dialog_exec(void* dialog) {
  auto* qt = static_cast<QDialog*>(dialog);
  return qt == nullptr ? 0 : qt->exec();
}

void sere_qt6_dialog_accept(void* dialog) {
  auto* qt = static_cast<QDialog*>(dialog);
  if (qt != nullptr) {
    qt->accept();
  }
}

void sere_qt6_dialog_reject(void* dialog) {
  auto* qt = static_cast<QDialog*>(dialog);
  if (qt != nullptr) {
    qt->reject();
  }
}

void* sere_qt6_window_central(void* window) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  return main == nullptr ? window : main->centralWidget();
}

void sere_qt6_window_set_central(void* window, void* widget) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  if (main != nullptr) {
    main->setCentralWidget(asW(widget));
  }
}

void* sere_qt6_menu_bar(void* window) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  return main == nullptr ? nullptr : static_cast<void*>(main->menuBar());
}

void* sere_qt6_status_bar(void* window) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  return main == nullptr ? nullptr : static_cast<void*>(main->statusBar());
}

void sere_qt6_status_message(void* bar, const char* text, int64_t len, int32_t ms) {
  auto* status = static_cast<QStatusBar*>(bar);
  if (status != nullptr) {
    status->showMessage(fromSere(text, len), ms);
  }
}

void* sere_qt6_tool_bar(void* window, const char* title, int64_t title_len) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  if (main == nullptr) {
    return nullptr;
  }
  return main->addToolBar(fromSere(title, title_len));
}

void sere_qt6_add_dock(void* window, void* widget, int32_t area) {
  auto* main = qobject_cast<QMainWindow*>(asW(window));
  if (main == nullptr || widget == nullptr) {
    return;
  }
  auto* dock = new QDockWidget(main);
  dock->setWidget(asW(widget));
  main->addDockWidget(static_cast<Qt::DockWidgetArea>(area == 0 ? Qt::LeftDockWidgetArea : area),
                      dock);
}

void sere_qt6_widget_set_title(void* widget, const char* title, int64_t title_len) {
  if (QWidget* qt = asW(widget)) {
    qt->setWindowTitle(fromSere(title, title_len));
  }
}

void sere_qt6_widget_title(void* widget, const char** out_data, int64_t* out_len) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->windowTitle().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_widget_resize(void* widget, int32_t width, int32_t height) {
  if (QWidget* qt = asW(widget)) {
    qt->resize(width, height);
  }
}

void sere_qt6_widget_move(void* widget, int32_t x, int32_t y) {
  if (QWidget* qt = asW(widget)) {
    qt->move(x, y);
  }
}

int32_t sere_qt6_widget_x(void* widget) {
  QWidget* qt = asW(widget);
  return qt == nullptr ? 0 : qt->x();
}

int32_t sere_qt6_widget_y(void* widget) {
  QWidget* qt = asW(widget);
  return qt == nullptr ? 0 : qt->y();
}

int32_t sere_qt6_widget_width(void* widget) {
  QWidget* qt = asW(widget);
  return qt == nullptr ? 0 : qt->width();
}

int32_t sere_qt6_widget_height(void* widget) {
  QWidget* qt = asW(widget);
  return qt == nullptr ? 0 : qt->height();
}

void sere_qt6_widget_show(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->show();
  }
}

void sere_qt6_widget_hide(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->hide();
  }
}

void sere_qt6_widget_close(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->close();
  }
}

void sere_qt6_widget_raise(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->raise();
  }
}

void sere_qt6_widget_lower(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->lower();
  }
}

void sere_qt6_widget_update(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->update();
  }
}

void sere_qt6_widget_focus(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->setFocus();
  }
}

void sere_qt6_widget_set_enabled(void* widget, int32_t enabled) {
  if (QWidget* qt = asW(widget)) {
    qt->setEnabled(enabled != 0);
  }
}

void sere_qt6_widget_set_visible(void* widget, int32_t visible) {
  if (QWidget* qt = asW(widget)) {
    qt->setVisible(visible != 0);
  }
}

void sere_qt6_widget_set_stylesheet(void* widget, const char* text, int64_t len) {
  if (QWidget* qt = asW(widget)) {
    qt->setStyleSheet(fromSere(text, len));
  }
}

void sere_qt6_widget_stylesheet(void* widget, const char** out_data, int64_t* out_len) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->styleSheet().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_widget_set_object_name(void* widget, const char* text, int64_t len) {
  if (QObject* qt = static_cast<QObject*>(widget)) {
    qt->setObjectName(fromSere(text, len));
  }
}

void sere_qt6_widget_set_property(void* widget, const char* key, int64_t key_len, const char* val,
                                  int64_t val_len) {
  if (QObject* qt = static_cast<QObject*>(widget)) {
    qt->setProperty(fromSere(key, key_len).toUtf8().constData(), fromSere(val, val_len));
    if (QWidget* w = asW(widget)) {
      w->style()->unpolish(w);
      w->style()->polish(w);
      w->update();
    }
  }
}

void sere_qt6_widget_set_tooltip(void* widget, const char* text, int64_t len) {
  if (QWidget* qt = asW(widget)) {
    qt->setToolTip(fromSere(text, len));
  }
}

void sere_qt6_widget_set_cursor(void* widget, int32_t shape) {
  if (QWidget* qt = asW(widget)) {
    qt->setCursor(QCursor(static_cast<Qt::CursorShape>(shape)));
  }
}

void sere_qt6_widget_set_font(void* widget, const char* family, int64_t family_len, int32_t size,
                              int32_t weight, int32_t italic) {
  applyFont(asW(widget), family, family_len, size, weight, italic);
}

void sere_qt6_widget_set_palette_color(void* widget, int32_t role, int32_t r, int32_t g, int32_t b,
                                       int32_t a) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  QPalette palette = qt->palette();
  palette.setColor(static_cast<QPalette::ColorRole>(role), QColor(r, g, b, a));
  qt->setPalette(palette);
  qt->setAutoFillBackground(true);
}

void sere_qt6_widget_set_min_size(void* widget, int32_t width, int32_t height) {
  if (QWidget* qt = asW(widget)) {
    qt->setMinimumSize(width, height);
  }
}

void sere_qt6_widget_set_max_size(void* widget, int32_t width, int32_t height) {
  if (QWidget* qt = asW(widget)) {
    qt->setMaximumSize(width, height);
  }
}

void sere_qt6_widget_set_fixed_size(void* widget, int32_t width, int32_t height) {
  if (QWidget* qt = asW(widget)) {
    qt->setFixedSize(width, height);
  }
}

void sere_qt6_widget_set_margins(void* widget, int32_t l, int32_t t, int32_t r, int32_t b) {
  if (QWidget* qt = asW(widget)) {
    qt->setContentsMargins(l, t, r, b);
  }
}

void sere_qt6_widget_set_size_policy(void* widget, int32_t h, int32_t v) {
  if (QWidget* qt = asW(widget)) {
    qt->setSizePolicy(static_cast<QSizePolicy::Policy>(h), static_cast<QSizePolicy::Policy>(v));
  }
}

void sere_qt6_widget_set_layout(void* widget, void* layout) {
  if (QWidget* qt = asW(widget)) {
    qt->setLayout(static_cast<QLayout*>(layout));
  }
}

void sere_qt6_widget_set_parent(void* widget, void* parent) {
  if (QWidget* qt = asW(widget)) {
    qt->setParent(asW(parent));
  }
}

void sere_qt6_widget_set_attr(void* widget, int32_t attr, int32_t on) {
  if (QWidget* qt = asW(widget)) {
    qt->setAttribute(static_cast<Qt::WidgetAttribute>(attr), on != 0);
  }
}

void sere_qt6_widget_set_flags(void* widget, int32_t flags) {
  if (QWidget* qt = asW(widget)) {
    qt->setWindowFlags(static_cast<Qt::WindowFlags>(flags));
  }
}

void sere_qt6_widget_set_modality(void* widget, int32_t modality) {
  if (QWidget* qt = asW(widget)) {
    qt->setWindowModality(static_cast<Qt::WindowModality>(modality));
  }
}

void sere_qt6_widget_set_icon(void* widget, const char* path, int64_t path_len) {
  if (auto* button = qobject_cast<QAbstractButton*>(asW(widget))) {
    button->setIcon(QIcon(fromSere(path, path_len)));
  }
}

void sere_qt6_widget_set_window_icon(void* widget, const char* path, int64_t path_len) {
  if (QWidget* qt = asW(widget)) {
    qt->setWindowIcon(QIcon(fromSere(path, path_len)));
  }
}

int32_t sere_qt6_take(void* obj, int32_t kind) {
  if (obj == nullptr || kind < 1 || kind >= 16) {
    return 0;
  }
  EventBox& events = box(obj);
  const int32_t value = events.flags[kind];
  events.flags[kind] = 0;
  return value;
}

int32_t sere_qt6_last_int(void* obj) { return box(obj).iValue; }

void sere_qt6_last_text(void* obj, const char** out_data, int64_t* out_len) {
  outOwned(box(obj).text, out_data, out_len);
}

void* sere_qt6_label_new(void* parent, const char* text, int64_t text_len) {
  auto* label = new QLabel(fromSere(text, text_len), asW(parent));
  label->setWordWrap(true);
  return label;
}

void sere_qt6_label_set_text(void* label, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QLabel*>(asW(label))) {
    qt->setText(fromSere(text, text_len));
  }
}

void sere_qt6_label_set_align(void* label, int32_t align) {
  if (auto* qt = qobject_cast<QLabel*>(asW(label))) {
    qt->setAlignment(static_cast<Qt::Alignment>(align));
  }
}

void sere_qt6_label_set_wrap(void* label, int32_t wrap) {
  if (auto* qt = qobject_cast<QLabel*>(asW(label))) {
    qt->setWordWrap(wrap != 0);
  }
}

void sere_qt6_label_set_pixmap(void* label, const char* path, int64_t path_len) {
  if (auto* qt = qobject_cast<QLabel*>(asW(label))) {
    qt->setPixmap(QPixmap(fromSere(path, path_len)));
  }
}

void* sere_qt6_button_new(void* parent, const char* text, int64_t text_len, int32_t kind) {
  QAbstractButton* button = nullptr;
  const QString label = fromSere(text, text_len);
  QWidget* host = asW(parent);
  if (kind == 1) {
    button = new QCheckBox(label, host);
  } else if (kind == 2) {
    button = new QRadioButton(label, host);
  } else if (kind == 3) {
    button = new QToolButton(host);
    button->setText(label);
  } else {
    button = new QPushButton(label, host);
  }
  connectButton(button);
  return button;
}

void sere_qt6_button_set_text(void* button, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QAbstractButton*>(asW(button))) {
    qt->setText(fromSere(text, text_len));
  }
}

void sere_qt6_button_set_checked(void* button, int32_t checked) {
  if (auto* qt = qobject_cast<QAbstractButton*>(asW(button))) {
    qt->setChecked(checked != 0);
  }
}

int32_t sere_qt6_button_checked(void* button) {
  auto* qt = qobject_cast<QAbstractButton*>(asW(button));
  return qt != nullptr && qt->isChecked() ? 1 : 0;
}

void sere_qt6_button_set_checkable(void* button, int32_t on) {
  if (auto* qt = qobject_cast<QAbstractButton*>(asW(button))) {
    qt->setCheckable(on != 0);
  }
}

void* sere_qt6_line_new(void* parent, const char* text, int64_t text_len) {
  auto* line = new QLineEdit(fromSere(text, text_len), asW(parent));
  QObject::connect(line, &QLineEdit::textChanged, [line](const QString& value) {
    ping(line, 3, 0, value);
  });
  QObject::connect(line, &QLineEdit::returnPressed, [line]() { ping(line, 4); });
  return line;
}

void sere_qt6_line_set_text(void* line, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->setText(fromSere(text, text_len));
  }
}

void sere_qt6_line_text(void* line, const char** out_data, int64_t* out_len) {
  auto* qt = qobject_cast<QLineEdit*>(asW(line));
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->text().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_line_set_placeholder(void* line, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->setPlaceholderText(fromSere(text, text_len));
  }
}

void sere_qt6_line_set_echo(void* line, int32_t mode) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->setEchoMode(static_cast<QLineEdit::EchoMode>(mode));
  }
}

void sere_qt6_line_set_read_only(void* line, int32_t on) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->setReadOnly(on != 0);
  }
}

void* sere_qt6_text_new(void* parent, const char* text, int64_t text_len, int32_t plain) {
  const QString value = fromSere(text, text_len);
  if (plain != 0) {
    auto* edit = new QPlainTextEdit(value, asW(parent));
    QObject::connect(edit, &QPlainTextEdit::textChanged, [edit]() {
      ping(edit, 3, 0, edit->toPlainText());
    });
    return edit;
  }
  auto* edit = new QTextEdit(value, asW(parent));
  QObject::connect(edit, &QTextEdit::textChanged, [edit]() {
    ping(edit, 3, 0, edit->toPlainText());
  });
  return edit;
}

void sere_qt6_text_set(void* edit, const char* text, int64_t text_len) {
  const QString value = fromSere(text, text_len);
  if (auto* plain = qobject_cast<QPlainTextEdit*>(asW(edit))) {
    plain->setPlainText(value);
    return;
  }
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    rich->setPlainText(value);
  }
}

void sere_qt6_text_get(void* edit, const char** out_data, int64_t* out_len) {
  if (auto* plain = qobject_cast<QPlainTextEdit*>(asW(edit))) {
    outOwned(plain->toPlainText().toUtf8().toStdString(), out_data, out_len);
    return;
  }
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    outOwned(rich->toPlainText().toUtf8().toStdString(), out_data, out_len);
    return;
  }
  outEmpty(out_data, out_len);
}

void* sere_qt6_spin_new(void* parent, int32_t is_double) {
  if (is_double != 0) {
    auto* spin = new QDoubleSpinBox(asW(parent));
    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     [spin](double value) { ping(spin, 5, static_cast<int32_t>(value)); });
    return spin;
  }
  auto* spin = new QSpinBox(asW(parent));
  QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                   [spin](int value) { ping(spin, 5, value); });
  return spin;
}

void sere_qt6_spin_set_range(void* spin, double lo, double hi) {
  if (auto* d = qobject_cast<QDoubleSpinBox*>(asW(spin))) {
    d->setRange(lo, hi);
    return;
  }
  if (auto* i = qobject_cast<QSpinBox*>(asW(spin))) {
    i->setRange(static_cast<int>(lo), static_cast<int>(hi));
  }
}

void sere_qt6_spin_set_value(void* spin, double value) {
  if (auto* d = qobject_cast<QDoubleSpinBox*>(asW(spin))) {
    d->setValue(value);
    return;
  }
  if (auto* i = qobject_cast<QSpinBox*>(asW(spin))) {
    i->setValue(static_cast<int>(value));
  }
}

double sere_qt6_spin_value(void* spin) {
  if (auto* d = qobject_cast<QDoubleSpinBox*>(asW(spin))) {
    return d->value();
  }
  if (auto* i = qobject_cast<QSpinBox*>(asW(spin))) {
    return static_cast<double>(i->value());
  }
  return 0.0;
}

void* sere_qt6_slider_new(void* parent, int32_t orient, int32_t kind) {
  const Qt::Orientation orientation = orient == 1 ? Qt::Vertical : Qt::Horizontal;
  if (kind == 1) {
    auto* dial = new QDial(asW(parent));
    QObject::connect(dial, &QDial::valueChanged, [dial](int value) { ping(dial, 5, value); });
    return dial;
  }
  auto* slider = new QSlider(orientation, asW(parent));
  QObject::connect(slider, &QSlider::valueChanged, [slider](int value) { ping(slider, 5, value); });
  return slider;
}

void sere_qt6_slider_set_range(void* slider, int32_t lo, int32_t hi) {
  if (auto* abs = qobject_cast<QAbstractSlider*>(asW(slider))) {
    abs->setRange(lo, hi);
  }
}

void sere_qt6_slider_set_value(void* slider, int32_t value) {
  if (auto* abs = qobject_cast<QAbstractSlider*>(asW(slider))) {
    abs->setValue(value);
  }
}

int32_t sere_qt6_slider_value(void* slider) {
  auto* abs = qobject_cast<QAbstractSlider*>(asW(slider));
  return abs == nullptr ? 0 : abs->value();
}

void* sere_qt6_progress_new(void* parent) { return new QProgressBar(asW(parent)); }

void sere_qt6_progress_set(void* bar, int32_t value) {
  if (auto* qt = qobject_cast<QProgressBar*>(asW(bar))) {
    qt->setValue(value);
  }
}

void sere_qt6_progress_set_range(void* bar, int32_t lo, int32_t hi) {
  if (auto* qt = qobject_cast<QProgressBar*>(asW(bar))) {
    qt->setRange(lo, hi);
  }
}

void* sere_qt6_combo_new(void* parent) {
  auto* combo = new QComboBox(asW(parent));
  combo->setEditable(false);
  QObject::connect(combo, qOverload<int>(&QComboBox::currentIndexChanged),
                   [combo](int index) { ping(combo, 6, index, combo->currentText()); });
  return combo;
}

void sere_qt6_combo_add(void* combo, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QComboBox*>(asW(combo))) {
    qt->addItem(fromSere(text, text_len));
  }
}

void sere_qt6_combo_clear(void* combo) {
  if (auto* qt = qobject_cast<QComboBox*>(asW(combo))) {
    qt->clear();
  }
}

int32_t sere_qt6_combo_index(void* combo) {
  auto* qt = qobject_cast<QComboBox*>(asW(combo));
  return qt == nullptr ? -1 : qt->currentIndex();
}

void sere_qt6_combo_set_index(void* combo, int32_t index) {
  if (auto* qt = qobject_cast<QComboBox*>(asW(combo))) {
    qt->setCurrentIndex(index);
  }
}

void sere_qt6_combo_text(void* combo, const char** out_data, int64_t* out_len) {
  auto* qt = qobject_cast<QComboBox*>(asW(combo));
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->currentText().toUtf8().toStdString(), out_data, out_len);
}

void* sere_qt6_list_new(void* parent) {
  auto* list = new QListWidget(asW(parent));
  QObject::connect(list, &QListWidget::currentRowChanged,
                   [list](int row) { ping(list, 8, row); });
  return list;
}

void sere_qt6_list_add(void* list, const char* text, int64_t text_len) {
  if (auto* qt = qobject_cast<QListWidget*>(asW(list))) {
    qt->addItem(fromSere(text, text_len));
  }
}

void sere_qt6_list_clear(void* list) {
  if (auto* qt = qobject_cast<QListWidget*>(asW(list))) {
    qt->clear();
  }
}

int32_t sere_qt6_list_row(void* list) {
  auto* qt = qobject_cast<QListWidget*>(asW(list));
  return qt == nullptr ? -1 : qt->currentRow();
}

void sere_qt6_list_item(void* list, int32_t row, const char** out_data, int64_t* out_len) {
  auto* qt = qobject_cast<QListWidget*>(asW(list));
  if (qt == nullptr || qt->item(row) == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(qt->item(row)->text().toUtf8().toStdString(), out_data, out_len);
}

void* sere_qt6_table_new(void* parent, int32_t rows, int32_t cols) {
  auto* table = new QTableWidget(rows, cols, asW(parent));
  table->horizontalHeader()->setStretchLastSection(true);
  QObject::connect(table, &QTableWidget::cellClicked, [table](int row, int col) {
    EventBox& events = box(table);
    events.iValue = row;
    events.iValue2 = col;
    ping(table, 12, row);
  });
  return table;
}

void sere_qt6_table_set(void* table, int32_t row, int32_t col, const char* text, int64_t text_len) {
  auto* qt = qobject_cast<QTableWidget*>(asW(table));
  if (qt == nullptr) {
    return;
  }
  qt->setItem(row, col, new QTableWidgetItem(fromSere(text, text_len)));
}

void sere_qt6_table_header(void* table, int32_t col, const char* text, int64_t text_len) {
  auto* qt = qobject_cast<QTableWidget*>(asW(table));
  if (qt != nullptr) {
    qt->setHorizontalHeaderItem(col, new QTableWidgetItem(fromSere(text, text_len)));
  }
}

void sere_qt6_table_set_rows(void* table, int32_t rows) {
  if (auto* qt = qobject_cast<QTableWidget*>(asW(table))) {
    qt->setRowCount(rows);
  }
}

int32_t sere_qt6_table_row(void* table) { return box(table).iValue; }

int32_t sere_qt6_table_col(void* table) { return box(table).iValue2; }

void* sere_qt6_tree_new(void* parent) {
  auto* tree = new QTreeWidget(asW(parent));
  tree->setHeaderHidden(true);
  QObject::connect(tree, &QTreeWidget::itemClicked, [tree](QTreeWidgetItem* item) {
    ping(tree, 8, 0, item == nullptr ? QString() : item->text(0));
  });
  return tree;
}

void sere_qt6_tree_add(void* tree, const char* text, int64_t text_len) {
  auto* qt = qobject_cast<QTreeWidget*>(asW(tree));
  if (qt != nullptr) {
    qt->addTopLevelItem(new QTreeWidgetItem(QStringList{fromSere(text, text_len)}));
  }
}

void* sere_qt6_tabs_new(void* parent) {
  auto* tabs = new QTabWidget(asW(parent));
  QObject::connect(tabs, &QTabWidget::currentChanged, [tabs](int index) { ping(tabs, 9, index); });
  return tabs;
}

void sere_qt6_tabs_add(void* tabs, void* page, const char* title, int64_t title_len) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->addTab(asW(page), fromSere(title, title_len));
  }
}

int32_t sere_qt6_tabs_index(void* tabs) {
  auto* qt = qobject_cast<QTabWidget*>(asW(tabs));
  return qt == nullptr ? 0 : qt->currentIndex();
}

void sere_qt6_tabs_set_index(void* tabs, int32_t index) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->setCurrentIndex(index);
  }
}

void* sere_qt6_group_new(void* parent, const char* title, int64_t title_len) {
  return new QGroupBox(fromSere(title, title_len), asW(parent));
}

void* sere_qt6_frame_new(void* parent, int32_t shape) {
  auto* frame = new QFrame(asW(parent));
  frame->setFrameShape(static_cast<QFrame::Shape>(shape));
  return frame;
}

void* sere_qt6_scroll_new(void* parent) { return new QScrollArea(asW(parent)); }

void sere_qt6_scroll_set(void* scroll, void* widget) {
  if (auto* qt = qobject_cast<QScrollArea*>(asW(scroll))) {
    qt->setWidget(asW(widget));
    qt->setWidgetResizable(true);
  }
}

void* sere_qt6_splitter_new(void* parent, int32_t orient) {
  return new QSplitter(orient == 1 ? Qt::Vertical : Qt::Horizontal, asW(parent));
}

void sere_qt6_splitter_add(void* splitter, void* widget) {
  if (auto* qt = qobject_cast<QSplitter*>(asW(splitter))) {
    qt->addWidget(asW(widget));
  }
}

void sere_qt6_splitter_sizes(void* splitter, int32_t a, int32_t b) {
  if (auto* qt = qobject_cast<QSplitter*>(asW(splitter))) {
    qt->setSizes({a, b});
  }
}

void* sere_qt6_stack_new(void* parent) { return new QStackedWidget(asW(parent)); }

void sere_qt6_stack_add(void* stack, void* widget) {
  if (auto* qt = qobject_cast<QStackedWidget*>(asW(stack))) {
    qt->addWidget(asW(widget));
  }
}

void sere_qt6_stack_set(void* stack, int32_t index) {
  if (auto* qt = qobject_cast<QStackedWidget*>(asW(stack))) {
    qt->setCurrentIndex(index);
  }
}

void* sere_qt6_lcd_new(void* parent) { return new QLCDNumber(asW(parent)); }

void sere_qt6_lcd_set(void* lcd, int32_t value) {
  if (auto* qt = qobject_cast<QLCDNumber*>(asW(lcd))) {
    qt->display(value);
  }
}

void* sere_qt6_calendar_new(void* parent) { return new QCalendarWidget(asW(parent)); }

void* sere_qt6_date_new(void* parent, int32_t time_edit) {
  if (time_edit != 0) {
    return new QTimeEdit(QTime::currentTime(), asW(parent));
  }
  return new QDateEdit(QDate::currentDate(), asW(parent));
}

void* sere_qt6_vbox_new(void* parent) { return new QVBoxLayout(layoutHost(parent)); }

void* sere_qt6_hbox_new(void* parent) { return new QHBoxLayout(layoutHost(parent)); }

void* sere_qt6_grid_new(void* parent) { return new QGridLayout(layoutHost(parent)); }

void* sere_qt6_form_new(void* parent) { return new QFormLayout(layoutHost(parent)); }

void sere_qt6_layout_add(void* layout, void* widget, int32_t stretch) {
  if (auto* box = qobject_cast<QBoxLayout*>(static_cast<QLayout*>(layout))) {
    box->addWidget(asW(widget), stretch);
    return;
  }
  if (QLayout* lay = static_cast<QLayout*>(layout)) {
    lay->addWidget(asW(widget));
  }
}

void sere_qt6_layout_add_stretch(void* layout, int32_t stretch) {
  if (auto* box = qobject_cast<QBoxLayout*>(static_cast<QLayout*>(layout))) {
    box->addStretch(stretch);
  }
}

void sere_qt6_layout_spacing(void* layout, int32_t spacing) {
  if (QLayout* lay = static_cast<QLayout*>(layout)) {
    lay->setSpacing(spacing);
  }
}

void sere_qt6_layout_margins(void* layout, int32_t l, int32_t t, int32_t r, int32_t b) {
  if (QLayout* lay = static_cast<QLayout*>(layout)) {
    lay->setContentsMargins(l, t, r, b);
  }
}

void sere_qt6_grid_add(void* layout, void* widget, int32_t row, int32_t col, int32_t rs,
                       int32_t cs) {
  if (auto* grid = qobject_cast<QGridLayout*>(static_cast<QLayout*>(layout))) {
    grid->addWidget(asW(widget), row, col, rs < 1 ? 1 : rs, cs < 1 ? 1 : cs);
  }
}

void sere_qt6_form_add(void* layout, const char* label, int64_t label_len, void* widget) {
  if (auto* form = qobject_cast<QFormLayout*>(static_cast<QLayout*>(layout))) {
    form->addRow(fromSere(label, label_len), asW(widget));
  }
}

void* sere_qt6_menu_add(void* bar, const char* title, int64_t title_len) {
  if (auto* menus = qobject_cast<QMenuBar*>(static_cast<QObject*>(bar))) {
    return menus->addMenu(fromSere(title, title_len));
  }
  if (auto* menu = qobject_cast<QMenu*>(static_cast<QObject*>(bar))) {
    return menu->addMenu(fromSere(title, title_len));
  }
  return nullptr;
}

void* sere_qt6_menu_action(void* menu, const char* text, int64_t text_len, const char* shortcut,
                           int64_t shortcut_len) {
  QMenu* qt = qobject_cast<QMenu*>(static_cast<QObject*>(menu));
  if (qt == nullptr) {
    return nullptr;
  }
  QAction* action = qt->addAction(fromSere(text, text_len));
  if (shortcut_len > 0) {
    action->setShortcut(QKeySequence(fromSere(shortcut, shortcut_len)));
  }
  QObject::connect(action, &QAction::triggered, [action]() { ping(action, 10); });
  return action;
}

void sere_qt6_menu_separator(void* menu) {
  if (auto* qt = qobject_cast<QMenu*>(static_cast<QObject*>(menu))) {
    qt->addSeparator();
  }
}

void sere_qt6_toolbar_add(void* bar, void* action) {
  if (auto* qt = qobject_cast<QToolBar*>(asW(bar))) {
    qt->addAction(static_cast<QAction*>(action));
  }
}

void* sere_qt6_timer_new(int32_t ms) {
  auto* timer = new QTimer();
  timer->setInterval(ms);
  QObject::connect(timer, &QTimer::timeout, [timer]() { ping(timer, 7); });
  return timer;
}

void sere_qt6_timer_start(void* timer) {
  if (auto* qt = static_cast<QTimer*>(timer)) {
    qt->start();
  }
}

void sere_qt6_timer_stop(void* timer) {
  if (auto* qt = static_cast<QTimer*>(timer)) {
    qt->stop();
  }
}

void sere_qt6_timer_set_interval(void* timer, int32_t ms) {
  if (auto* qt = static_cast<QTimer*>(timer)) {
    qt->setInterval(ms);
  }
}

void* sere_qt6_shortcut_new(void* parent, const char* seq, int64_t seq_len) {
  auto* shortcut = new QShortcut(QKeySequence(fromSere(seq, seq_len)), asW(parent));
  QObject::connect(shortcut, &QShortcut::activated, [shortcut]() { ping(shortcut, 10); });
  return shortcut;
}

void sere_qt6_file_open(void* parent, const char* caption, int64_t cap_len, const char* filter,
                        int64_t filter_len, const char** out_data, int64_t* out_len) {
  const QString path = QFileDialog::getOpenFileName(asW(parent), fromSere(caption, cap_len),
                                                    QString(), fromSere(filter, filter_len));
  outOwned(path.toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_file_save(void* parent, const char* caption, int64_t cap_len, const char* filter,
                        int64_t filter_len, const char** out_data, int64_t* out_len) {
  const QString path = QFileDialog::getSaveFileName(asW(parent), fromSere(caption, cap_len),
                                                    QString(), fromSere(filter, filter_len));
  outOwned(path.toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_dir_open(void* parent, const char* caption, int64_t cap_len, const char** out_data,
                       int64_t* out_len) {
  const QString path = QFileDialog::getExistingDirectory(asW(parent), fromSere(caption, cap_len));
  outOwned(path.toUtf8().toStdString(), out_data, out_len);
}

int32_t sere_qt6_color_dialog(void* parent, int32_t r, int32_t g, int32_t b, int32_t* out_r,
                              int32_t* out_g, int32_t* out_b) {
  const QColor chosen = QColorDialog::getColor(QColor(r, g, b), asW(parent), "Color");
  if (!chosen.isValid()) {
    return 0;
  }
  if (out_r != nullptr) {
    *out_r = chosen.red();
  }
  if (out_g != nullptr) {
    *out_g = chosen.green();
  }
  if (out_b != nullptr) {
    *out_b = chosen.blue();
  }
  return 1;
}

void sere_qt6_font_dialog(void* parent, const char** out_data, int64_t* out_len) {
  bool ok = false;
  const QFont font = QFontDialog::getFont(&ok, asW(parent));
  if (!ok) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(font.toString().toUtf8().toStdString(), out_data, out_len);
}

int32_t sere_qt6_message(void* parent, int32_t kind, const char* title, int64_t title_len,
                         const char* text, int64_t text_len) {
  const QString qTitle = fromSere(title, title_len);
  const QString qText = fromSere(text, text_len);
  QWidget* host = asW(parent);
  if (kind == 1) {
    return QMessageBox::information(host, qTitle, qText);
  }
  if (kind == 2) {
    return QMessageBox::warning(host, qTitle, qText);
  }
  if (kind == 3) {
    return QMessageBox::critical(host, qTitle, qText);
  }
  if (kind == 4) {
    return QMessageBox::question(host, qTitle, qText);
  }
  QMessageBox::about(host, qTitle, qText);
  return 0;
}

void sere_qt6_input_text(void* parent, const char* title, int64_t title_len, const char* label,
                         int64_t label_len, const char** out_data, int64_t* out_len) {
  bool ok = false;
  const QString value = QInputDialog::getText(asW(parent), fromSere(title, title_len),
                                              fromSere(label, label_len), QLineEdit::Normal,
                                              QString(), &ok);
  if (!ok) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(value.toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_settings_set(const char* key, int64_t key_len, const char* val, int64_t val_len) {
  QSettings settings;
  settings.setValue(fromSere(key, key_len), fromSere(val, val_len));
}

void sere_qt6_settings_get(const char* key, int64_t key_len, const char** out_data,
                           int64_t* out_len) {
  QSettings settings;
  outOwned(settings.value(fromSere(key, key_len)).toString().toUtf8().toStdString(), out_data,
           out_len);
}

void* sere_qt6_tray_new(const char* tip, int64_t tip_len) {
  auto* tray = new QSystemTrayIcon();
  tray->setToolTip(fromSere(tip, tip_len));
  const QIcon icon = sereDefaultWindowIcon();
  if (!icon.isNull()) {
    tray->setIcon(icon);
  }
  tray->show();
  return tray;
}

void sere_qt6_tray_show(void* tray, const char* title, int64_t title_len, const char* msg,
                        int64_t msg_len) {
  if (auto* qt = static_cast<QSystemTrayIcon*>(tray)) {
    qt->showMessage(fromSere(title, title_len), fromSere(msg, msg_len));
  }
}

void sere_qt6_widget_opacity(void* widget, double value) {
  if (QWidget* qt = asW(widget)) {
    qt->setWindowOpacity(value);
  }
}

int32_t sere_qt6_widget_enabled(void* widget) {
  QWidget* qt = asW(widget);
  return qt != nullptr && qt->isEnabled() ? 1 : 0;
}

int32_t sere_qt6_widget_visible(void* widget) {
  QWidget* qt = asW(widget);
  return qt != nullptr && qt->isVisible() ? 1 : 0;
}

void sere_qt6_widget_set_state(void* widget, int32_t state) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  if (state == 1) {
    qt->showMinimized();
  } else if (state == 2) {
    qt->showMaximized();
  } else if (state == 3) {
    qt->showFullScreen();
  } else {
    qt->showNormal();
  }
}

int32_t sere_qt6_widget_state(void* widget) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return 0;
  }
  if (qt->isFullScreen()) {
    return 3;
  }
  if (qt->isMaximized()) {
    return 2;
  }
  if (qt->isMinimized()) {
    return 1;
  }
  return 0;
}

void sere_qt6_widget_center(void* widget) {
  QWidget* qt = asW(widget);
  QScreen* screen = QGuiApplication::primaryScreen();
  if (qt == nullptr || screen == nullptr) {
    return;
  }
  const QRect area = screen->availableGeometry();
  qt->move(area.center().x() - qt->width() / 2, area.center().y() - qt->height() / 2);
}

void sere_qt6_widget_adjust(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->adjustSize();
  }
}

void sere_qt6_widget_activate(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->activateWindow();
    qt->raise();
  }
}

void sere_qt6_widget_set_status_tip(void* widget, const char* text, int64_t len) {
  if (QWidget* qt = asW(widget)) {
    qt->setStatusTip(fromSere(text, len));
  }
}

void sere_qt6_widget_set_whats_this(void* widget, const char* text, int64_t len) {
  if (QWidget* qt = asW(widget)) {
    qt->setWhatsThis(fromSere(text, len));
  }
}

void sere_qt6_widget_set_focus_policy(void* widget, int32_t policy) {
  if (QWidget* qt = asW(widget)) {
    qt->setFocusPolicy(static_cast<Qt::FocusPolicy>(policy));
  }
}

void sere_qt6_widget_set_context_policy(void* widget, int32_t policy) {
  if (QWidget* qt = asW(widget)) {
    qt->setContextMenuPolicy(static_cast<Qt::ContextMenuPolicy>(policy));
  }
}

void sere_qt6_widget_set_accept_drops(void* widget, int32_t on) {
  if (QWidget* qt = asW(widget)) {
    qt->setAcceptDrops(on != 0);
  }
}

void sere_qt6_widget_set_mouse_tracking(void* widget, int32_t on) {
  if (QWidget* qt = asW(widget)) {
    qt->setMouseTracking(on != 0);
  }
}

void sere_qt6_effect_opacity(void* widget, double value) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* effect = new QGraphicsOpacityEffect(qt);
  effect->setOpacity(value);
  qt->setGraphicsEffect(effect);
}

void sere_qt6_effect_blur(void* widget, double radius) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* effect = new QGraphicsBlurEffect(qt);
  effect->setBlurRadius(radius);
  qt->setGraphicsEffect(effect);
}

void sere_qt6_effect_shadow(void* widget, int32_t dx, int32_t dy, int32_t blur, int32_t r, int32_t g,
                            int32_t b, int32_t a) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* effect = new QGraphicsDropShadowEffect(qt);
  effect->setOffset(dx, dy);
  effect->setBlurRadius(blur);
  effect->setColor(QColor(r, g, b, a));
  qt->setGraphicsEffect(effect);
}

void sere_qt6_effect_colorize(void* widget, int32_t r, int32_t g, int32_t b, double strength) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* effect = new QGraphicsColorizeEffect(qt);
  effect->setColor(QColor(r, g, b));
  effect->setStrength(strength);
  qt->setGraphicsEffect(effect);
}

void sere_qt6_effect_clear(void* widget) {
  if (QWidget* qt = asW(widget)) {
    qt->setGraphicsEffect(nullptr);
  }
}

void sere_qt6_anim_opacity(void* widget, double from, double to, int32_t ms) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* anim = new QPropertyAnimation(qt, "windowOpacity", qt);
  anim->setDuration(ms < 0 ? 0 : ms);
  anim->setStartValue(from);
  anim->setEndValue(to);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void sere_qt6_anim_move(void* widget, int32_t x, int32_t y, int32_t w, int32_t h, int32_t ms) {
  QWidget* qt = asW(widget);
  if (qt == nullptr) {
    return;
  }
  auto* anim = new QPropertyAnimation(qt, "geometry", qt);
  anim->setDuration(ms < 0 ? 0 : ms);
  anim->setStartValue(qt->geometry());
  anim->setEndValue(QRect(x, y, w, h));
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void sere_qt6_text_set_html(void* edit, const char* text, int64_t len) {
  const QString value = fromSere(text, len);
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    rich->setHtml(value);
  }
}

void sere_qt6_text_html(void* edit, const char** out_data, int64_t* out_len) {
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    outOwned(rich->toHtml().toUtf8().toStdString(), out_data, out_len);
    return;
  }
  outEmpty(out_data, out_len);
}

void sere_qt6_text_set_markdown(void* edit, const char* text, int64_t len) {
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    rich->setMarkdown(fromSere(text, len));
  }
}

void sere_qt6_text_append(void* edit, const char* text, int64_t len) {
  const QString value = fromSere(text, len);
  if (auto* plain = qobject_cast<QPlainTextEdit*>(asW(edit))) {
    plain->appendPlainText(value);
    return;
  }
  if (auto* rich = qobject_cast<QTextEdit*>(asW(edit))) {
    rich->append(value);
  }
}

void sere_qt6_label_open_links(void* label, int32_t on) {
  if (auto* qt = qobject_cast<QLabel*>(asW(label))) {
    qt->setTextFormat(Qt::RichText);
    qt->setOpenExternalLinks(on != 0);
    qt->setTextInteractionFlags(Qt::TextBrowserInteraction);
  }
}

void sere_qt6_line_max_len(void* line, int32_t n) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->setMaxLength(n);
  }
}

void sere_qt6_line_select_all(void* line) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->selectAll();
  }
}

void sere_qt6_line_clear(void* line) {
  if (auto* qt = qobject_cast<QLineEdit*>(asW(line))) {
    qt->clear();
  }
}

void sere_qt6_line_completer(void* line, const char* items, int64_t len) {
  auto* qt = qobject_cast<QLineEdit*>(asW(line));
  if (qt == nullptr) {
    return;
  }
  auto* completer = new QCompleter(fromSere(items, len).split('\n'), qt);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  qt->setCompleter(completer);
}

void sere_qt6_combo_editable(void* combo, int32_t on) {
  if (auto* qt = qobject_cast<QComboBox*>(asW(combo))) {
    qt->setEditable(on != 0);
  }
}

int32_t sere_qt6_combo_count(void* combo) {
  auto* qt = qobject_cast<QComboBox*>(asW(combo));
  return qt == nullptr ? 0 : qt->count();
}

int32_t sere_qt6_list_count(void* list) {
  auto* qt = qobject_cast<QListWidget*>(asW(list));
  return qt == nullptr ? 0 : qt->count();
}

void sere_qt6_list_set_row(void* list, int32_t row) {
  if (auto* qt = qobject_cast<QListWidget*>(asW(list))) {
    qt->setCurrentRow(row);
  }
}

void sere_qt6_table_get(void* table, int32_t row, int32_t col, const char** out_data,
                        int64_t* out_len) {
  auto* qt = qobject_cast<QTableWidget*>(asW(table));
  if (qt == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  QTableWidgetItem* item = qt->item(row, col);
  if (item == nullptr) {
    outEmpty(out_data, out_len);
    return;
  }
  outOwned(item->text().toUtf8().toStdString(), out_data, out_len);
}

void sere_qt6_table_cols(void* table, int32_t cols) {
  if (auto* qt = qobject_cast<QTableWidget*>(asW(table))) {
    qt->setColumnCount(cols);
  }
}

void sere_qt6_table_stretch(void* table, int32_t on) {
  auto* qt = qobject_cast<QTableWidget*>(asW(table));
  if (qt != nullptr && qt->horizontalHeader() != nullptr) {
    qt->horizontalHeader()->setStretchLastSection(on != 0);
  }
}

void sere_qt6_tree_clear(void* tree) {
  if (auto* qt = qobject_cast<QTreeWidget*>(asW(tree))) {
    qt->clear();
  }
}

void sere_qt6_tabs_pos(void* tabs, int32_t pos) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->setTabPosition(static_cast<QTabWidget::TabPosition>(pos));
  }
}

void sere_qt6_tabs_closable(void* tabs, int32_t on) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->setTabsClosable(on != 0);
  }
}

void sere_qt6_tabs_movable(void* tabs, int32_t on) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->setMovable(on != 0);
  }
}

void sere_qt6_tabs_document(void* tabs, int32_t on) {
  if (auto* qt = qobject_cast<QTabWidget*>(asW(tabs))) {
    qt->setDocumentMode(on != 0);
  }
}

void sere_qt6_scroll_resizable(void* scroll, int32_t on) {
  if (auto* qt = qobject_cast<QScrollArea*>(asW(scroll))) {
    qt->setWidgetResizable(on != 0);
  }
}

void sere_qt6_progress_text(void* bar, int32_t on) {
  if (auto* qt = qobject_cast<QProgressBar*>(asW(bar))) {
    qt->setTextVisible(on != 0);
  }
}

void sere_qt6_spin_suffix(void* spin, const char* text, int64_t len) {
  const QString value = fromSere(text, len);
  if (auto* d = qobject_cast<QDoubleSpinBox*>(asW(spin))) {
    d->setSuffix(value);
    return;
  }
  if (auto* i = qobject_cast<QSpinBox*>(asW(spin))) {
    i->setSuffix(value);
  }
}

void sere_qt6_slider_ticks(void* slider, int32_t pos, int32_t interval) {
  if (auto* qt = qobject_cast<QSlider*>(asW(slider))) {
    qt->setTickPosition(static_cast<QSlider::TickPosition>(pos));
    qt->setTickInterval(interval);
  }
}

void sere_qt6_group_checkable(void* group, int32_t on) {
  if (auto* qt = qobject_cast<QGroupBox*>(asW(group))) {
    qt->setCheckable(on != 0);
  }
}

void sere_qt6_window_dock_nesting(void* window, int32_t on) {
  if (auto* main = qobject_cast<QMainWindow*>(asW(window))) {
    main->setDockNestingEnabled(on != 0);
  }
}

int32_t sere_qt6_color_pick(void* parent, int32_t r, int32_t g, int32_t b) {
  const QColor chosen =
      QColorDialog::getColor(QColor(r, g, b), asW(parent), "Color", QColorDialog::ShowAlphaChannel);
  if (!chosen.isValid()) {
    return -1;
  }
  return (chosen.red() << 16) | (chosen.green() << 8) | chosen.blue();
}

void* sere_qt6_vbox_bare(void) { return new QVBoxLayout(); }

void* sere_qt6_hbox_bare(void) { return new QHBoxLayout(); }

void sere_qt6_layout_add_layout(void* layout, void* child, int32_t stretch) {
  if (auto* box = qobject_cast<QBoxLayout*>(static_cast<QLayout*>(layout))) {
    box->addLayout(static_cast<QLayout*>(child), stretch);
  }
}

void* sere_qt6_browser_new(void* parent) {
  auto* browser = new QTextBrowser(asW(parent));
  browser->setOpenExternalLinks(true);
  return browser;
}

void* sere_qt6_toolbox_new(void* parent) { return new QToolBox(asW(parent)); }

void sere_qt6_toolbox_add(void* box, void* page, const char* title, int64_t len) {
  if (auto* qt = qobject_cast<QToolBox*>(asW(box))) {
    qt->addItem(asW(page), fromSere(title, len));
  }
}

void* sere_qt6_mdi_new(void* parent) {
  auto* mdi = new QMdiArea(asW(parent));
  mdi->setViewMode(QMdiArea::TabbedView);
  mdi->setTabsClosable(true);
  mdi->setTabsMovable(true);
  return mdi;
}

void* sere_qt6_mdi_add(void* mdi, void* widget, const char* title, int64_t len) {
  auto* area = qobject_cast<QMdiArea*>(asW(mdi));
  if (area == nullptr) {
    return nullptr;
  }
  QMdiSubWindow* sub = area->addSubWindow(asW(widget));
  sub->setWindowTitle(fromSere(title, len));
  applySereDefaultIcon(sub);
  sub->show();
  return sub;
}

void* sere_qt6_splash_new(const char* text, int64_t len) {
  QPixmap pix(520, 300);
  pix.fill(QColor(32, 32, 36));
  auto* splash = new QSplashScreen(pix);
  splash->showMessage(fromSere(text, len), Qt::AlignHCenter | Qt::AlignBottom, Qt::white);
  applySereDefaultIcon(splash);
  splash->show();
  return splash;
}

void sere_qt6_splash_finish(void* splash, void* window) {
  if (auto* qt = static_cast<QSplashScreen*>(splash)) {
    qt->finish(asW(window));
  }
}

void* sere_qt6_fontcombo_new(void* parent) { return new QFontComboBox(asW(parent)); }

void* sere_qt6_buttonbox_new(void* parent) { return new QDialogButtonBox(asW(parent)); }

void* sere_qt6_buttonbox_add(void* box, const char* text, int64_t len, int32_t role) {
  auto* qt = qobject_cast<QDialogButtonBox*>(asW(box));
  if (qt == nullptr) {
    return nullptr;
  }
  QPushButton* button =
      qt->addButton(fromSere(text, len), static_cast<QDialogButtonBox::ButtonRole>(role));
  connectButton(button);
  return button;
}

void sere_qt6_buttonbox_std(void* box, int32_t buttons) {
  if (auto* qt = qobject_cast<QDialogButtonBox*>(asW(box))) {
    qt->setStandardButtons(static_cast<QDialogButtonBox::StandardButtons>(buttons));
  }
}

void sere_qt6_menu_popup(void* menu, int32_t x, int32_t y) {
  if (auto* qt = qobject_cast<QMenu*>(static_cast<QObject*>(menu))) {
    qt->popup(QPoint(x, y));
  }
}

void sere_qt6_action_set_checkable(void* action, int32_t on) {
  if (auto* qt = static_cast<QAction*>(action)) {
    qt->setCheckable(on != 0);
  }
}

int32_t sere_qt6_action_checked(void* action) {
  auto* qt = static_cast<QAction*>(action);
  return qt != nullptr && qt->isChecked() ? 1 : 0;
}

void sere_qt6_action_set_enabled(void* action, int32_t on) {
  if (auto* qt = static_cast<QAction*>(action)) {
    qt->setEnabled(on != 0);
  }
}

void sere_qt6_action_set_text(void* action, const char* text, int64_t len) {
  if (auto* qt = static_cast<QAction*>(action)) {
    qt->setText(fromSere(text, len));
  }
}

void sere_qt6_action_set_shortcut(void* action, const char* seq, int64_t len) {
  if (auto* qt = static_cast<QAction*>(action)) {
    qt->setShortcut(QKeySequence(fromSere(seq, len)));
  }
}

void sere_qt6_app_set_palette_accent(void* app, int32_t r, int32_t g, int32_t b) {
  auto* qt = static_cast<QApplication*>(app == nullptr ? QApplication::instance() : app);
  if (qt == nullptr) {
    return;
  }
  QPalette palette = darkPalette();
  palette.setColor(QPalette::Highlight, QColor(r, g, b));
  palette.setColor(QPalette::Link, QColor(r, g, b));
  qt->setStyle("Fusion");
  qt->setPalette(palette);
}

void sere_qt6_app_beep(void) { QApplication::beep(); }

void sere_qt6_app_about_qt(void* parent) { QMessageBox::aboutQt(asW(parent)); }

void sere_qt6_calendar_iso(void* cal, const char** out_data, int64_t* out_len) {
  if (auto* qt = qobject_cast<QCalendarWidget*>(asW(cal))) {
    outOwned(qt->selectedDate().toString(Qt::ISODate).toUtf8().toStdString(), out_data, out_len);
    return;
  }
  outEmpty(out_data, out_len);
}

void sere_qt6_date_iso(void* edit, const char** out_data, int64_t* out_len) {
  if (auto* date = qobject_cast<QDateEdit*>(asW(edit))) {
    outOwned(date->date().toString(Qt::ISODate).toUtf8().toStdString(), out_data, out_len);
    return;
  }
  if (auto* time = qobject_cast<QTimeEdit*>(asW(edit))) {
    outOwned(time->time().toString(Qt::ISODate).toUtf8().toStdString(), out_data, out_len);
    return;
  }
  outEmpty(out_data, out_len);
}

void sere_qt6_date_set_iso(void* edit, const char* text, int64_t len) {
  const QString value = fromSere(text, len);
  if (auto* date = qobject_cast<QDateEdit*>(asW(edit))) {
    date->setDate(QDate::fromString(value, Qt::ISODate));
    return;
  }
  if (auto* time = qobject_cast<QTimeEdit*>(asW(edit))) {
    time->setTime(QTime::fromString(value, Qt::ISODate));
  }
}

#endif
}
