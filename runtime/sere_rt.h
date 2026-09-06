/// @file sere_rt.h
/// C ABI for allocation, unique/shared pointers, lists, and I/O.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char* data;
  int64_t len;
} SereStr;

typedef struct SereList {
  void* data;
  int64_t len;
  int64_t cap;
  int64_t stride;
} SereList;

const char* sere_input(const char* prompt, int64_t len);
void sere_print_str(const char* data, int64_t len);
void sere_write(const char* data, int64_t len);
void sere_write_nl(void);
void sere_write_i32(int32_t value);
void sere_write_i64(int64_t value);
void sere_write_bool(int8_t value);
void sere_write_ptr(const void* pointer);
void sere_write_f64(double value);
const char* sere_str_i32_data(int32_t value, int64_t* out_len);
const char* sere_str_i64_data(int64_t value, int64_t* out_len);
const char* sere_str_bool_data(int8_t value, int64_t* out_len);
const char* sere_str_ptr_data(const void* pointer, int64_t* out_len);
const char* sere_str_f64_data(double value, int64_t* out_len);
const char* sere_str_repr_data(const char* data, int64_t len, int64_t* out_len);
const char* sere_str_concat_data(
    const char* left, int64_t left_len, const char* right, int64_t right_len, int64_t* out_len);
void* sere_alloc(uint64_t size);
void sere_free(void* pointer);
void* sere_gc_alloc(uint64_t size);
void sere_gc_free(void* pointer);
void sere_gc_collect(void);
void sere_gc_name(const char** out_data, int64_t* out_len);
int32_t sere_gc_use(const char* name, int64_t name_len);
void sere_gc_add_root(void* pointer);
void sere_gc_remove_root(void* pointer);
void sere_gc_retain(void* pointer);
void sere_gc_release(void* pointer);
int64_t sere_gc_bytes_in_use(void);
int64_t sere_gc_bytes_allocated(void);
int64_t sere_gc_live_blocks(void);
int64_t sere_gc_collections(void);
void sere_mem_copy(void* dest, const void* src, int64_t size);
void sere_mem_set(void* dest, int32_t value, int64_t size);
int32_t sere_mem_eq(const void* left, const void* right, int64_t size);
void* sere_arena_new(int64_t cap);
void* sere_arena_alloc(void* arena, int64_t size);
void sere_arena_reset(void* arena);
void sere_arena_destroy(void* arena);
void* sere_pool_new(int64_t block_size, int64_t blocks);
void* sere_pool_alloc(void* pool);
void sere_pool_release(void* pool, void* pointer);
void sere_pool_destroy(void* pool);
void* sere_shared_new(uint64_t size);
void sere_shared_retain(void* payload);
void sere_shared_release(void* payload);
int64_t sere_list_len(void* list);
void* sere_list_new(int64_t stride);
void* sere_array_new(int64_t stride, int64_t length);
void sere_list_push(void* list, const void* item);
void sere_list_insert(void* list, int64_t index, const void* item);
void sere_list_remove(void* list, int64_t index);
void sere_list_pop(void* list, void* out_item);
void sere_list_pop_at(void* list, int64_t index, void* out_item);
int32_t sere_list_remove_value(void* list, const void* item);
int64_t sere_list_index_of(void* list, const void* item);
int64_t sere_list_count(void* list, const void* item);
void sere_list_clear(void* list);
void* sere_list_copy(void* list);
void sere_list_reverse(void* list);
void sere_list_extend(void* list, void* other);
void* sere_list_item(void* list, int64_t index);
void* sere_list_slice(void* list, int64_t start, int64_t end, int32_t has_start, int32_t has_end);
void* sere_list_from_argv(int argc, char** argv);
void* sere_dict_new(int64_t key_stride, int64_t val_stride, int32_t key_kind);
void sere_dict_set(void* dict, const void* key, const void* value);
int32_t sere_dict_get(void* dict, const void* key, void* out_value);
int32_t sere_dict_del(void* dict, const void* key);
int64_t sere_dict_len(void* dict);
int32_t sere_dict_has(void* dict, const void* key);
void sere_dict_clear(void* dict);
void* sere_dict_copy(void* dict);

// Cooperative async executor (see sere_async.c). Wakers are scheduled with a
// function pointer + opaque argument; generated coroutine glue posts wakeups
// through these entry points.
void sere_async_post(void (*fn)(void* arg), void* arg);
void sere_async_sleep_ms(void (*fn)(void* arg), void* arg, int64_t ms);
void sere_async_run(void);
int64_t sere_async_now_ms(void);

void sere_str_index(
    const char* data, int64_t len, int64_t index, const char** out_data, int64_t* out_len);
void sere_str_slice(const char* data,
                    int64_t len,
                    int64_t start,
                    int64_t end,
                    int32_t has_start,
                    int32_t has_end,
                    const char** out_data,
                    int64_t* out_len);
int32_t sere_str_contains(const char* hay, int64_t hay_len, const char* needle, int64_t needle_len);
int32_t sere_str_eq(const char* left, int64_t left_len, const char* right, int64_t right_len);
int32_t sere_str_cmp(const char* left, int64_t left_len, const char* right, int64_t right_len);
void sere_str_repeat(
    const char* data, int64_t len, int64_t count, const char** out_data, int64_t* out_len);
void sere_raise(const char* type, const char* message, int64_t message_len);
int32_t sere_has_error(void);
void sere_clear_error(void);
int32_t sere_error_isa(const char* name);
const char* sere_error_type(void);
const char* sere_error_message(int64_t* out_len);
void sere_panic(const char* message, int64_t len);
int32_t
sere_parse_int(const char* data, int64_t len, int32_t bits, int32_t is_signed, int64_t* out);
int32_t sere_parse_float(const char* data, int64_t len, int32_t is_f32, double* out);
int32_t sere_parse_bool(const char* data, int64_t len, int32_t* out);
int32_t sere_parse_none(const char* data, int64_t len);

int32_t sere_list_contains(void* list, const void* item);
void* sere_list_concat(void* left, void* right);

void sere_io_read_line(const char** out_data, int64_t* out_len);
void sere_io_eprint(const char* data, int64_t len);
void sere_io_eprint_nl(void);

void sere_fs_read_text(const char* path, int64_t path_len, const char** out_data, int64_t* out_len);
int32_t sere_fs_write_text(const char* path, int64_t path_len, const char* data, int64_t data_len);
int32_t sere_fs_exists(const char* path, int64_t path_len);
int32_t sere_fs_is_file(const char* path, int64_t path_len);
int32_t sere_fs_is_dir(const char* path, int64_t path_len);
int32_t sere_fs_remove(const char* path, int64_t path_len);
int32_t sere_fs_mkdir(const char* path, int64_t path_len);

void sere_path_join(const char* left,
                    int64_t left_len,
                    const char* right,
                    int64_t right_len,
                    const char** out_data,
                    int64_t* out_len);
void sere_path_dirname(const char* path, int64_t path_len, const char** out_data, int64_t* out_len);
void sere_path_basename(const char* path,
                        int64_t path_len,
                        const char** out_data,
                        int64_t* out_len);
void sere_path_ext(const char* path, int64_t path_len, const char** out_data, int64_t* out_len);
int32_t sere_path_is_abs(const char* path, int64_t path_len);

void sere_os_getcwd(const char** out_data, int64_t* out_len);
void sere_os_getenv(const char* name, int64_t name_len, const char** out_data, int64_t* out_len);
void* sere_os_listdir(const char* path, int64_t path_len);
void sere_os_exit(int32_t code);

int64_t sere_time_now_ms(void);
void sere_time_sleep_ms(int32_t ms);

void sere_string_upper(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_string_lower(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_string_strip(const char* data, int64_t len, const char** out_data, int64_t* out_len);
int32_t
sere_string_starts_with(const char* data, int64_t len, const char* prefix, int64_t prefix_len);
int32_t
sere_string_ends_with(const char* data, int64_t len, const char* suffix, int64_t suffix_len);
void sere_string_repeat(
    const char* data, int64_t len, int64_t count, const char** out_data, int64_t* out_len);

int32_t
sere_re_is_match(const char* pattern, int64_t pattern_len, const char* text, int64_t text_len);
void sere_re_find(const char* pattern,
                  int64_t pattern_len,
                  const char* text,
                  int64_t text_len,
                  const char** out_data,
                  int64_t* out_len);
void sere_re_replace(const char* pattern,
                     int64_t pattern_len,
                     const char* text,
                     int64_t text_len,
                     const char* repl,
                     int64_t repl_len,
                     const char** out_data,
                     int64_t* out_len);

double sere_math_sqrt(double value);
double sere_math_sin(double value);
double sere_math_cos(double value);
double sere_math_tan(double value);
double sere_math_asin(double value);
double sere_math_acos(double value);
double sere_math_atan(double value);
double sere_math_atan2(double y, double x);
double sere_math_sinh(double value);
double sere_math_cosh(double value);
double sere_math_tanh(double value);
double sere_math_exp(double value);
double sere_math_expm1(double value);
double sere_math_log(double value);
double sere_math_log2(double value);
double sere_math_log10(double value);
double sere_math_log1p(double value);
double sere_math_hypot(double x, double y);
double sere_math_fmod(double x, double y);
double sere_math_copysign(double mag, double sign);
double sere_math_round(double value);
double sere_math_trunc(double value);
double sere_math_fmin(double left, double right);
double sere_math_fmax(double left, double right);
double sere_math_clamp(double value, double low, double high);
double sere_math_lerp(double from, double to, double t);
double sere_math_radians(double degrees);
double sere_math_degrees(double radians);
double sere_math_pi(void);
double sere_math_tau(void);
double sere_math_e(void);
double sere_math_inf(void);
double sere_math_nan(void);
int32_t sere_math_isnan(double value);
int32_t sere_math_isinf(double value);
int32_t sere_math_isfinite(double value);
double sere_math_abs(double value);
double sere_math_floor(double value);
double sere_math_ceil(double value);
double sere_math_pow(double base, double exp);

void* sere_f64_full(int64_t length, double fill);
void* sere_f64_copy(void* values);
void* sere_f64_reverse(void* values);
void* sere_f64_concat(void* left, void* right);
void* sere_f64_add(void* left, void* right);
void* sere_f64_sub(void* left, void* right);
void* sere_f64_mul(void* left, void* right);
void* sere_f64_div(void* left, void* right);
void* sere_f64_scale(void* values, double factor);
void* sere_f64_abs(void* values);
void* sere_f64_clip(void* values, double low, double high);
void* sere_f64_linspace(double start, double stop, int64_t count);
double sere_f64_sum(void* values);
double sere_f64_mean(void* values);
double sere_f64_var(void* values);
double sere_f64_min(void* values);
double sere_f64_max(void* values);
int64_t sere_f64_argmin(void* values);
int64_t sere_f64_argmax(void* values);
double sere_f64_dot(void* left, void* right);
void* sere_f64_outer(void* left, void* right);
void* sere_f64_normalize(void* values);
void* sere_f64_cross3(void* left, void* right);

void* sere_mat_identity(int64_t n);
void* sere_mat_mul(void* left,
                   int64_t left_rows,
                   int64_t left_cols,
                   void* right,
                   int64_t right_rows,
                   int64_t right_cols);
void* sere_mat_transpose(void* values, int64_t rows, int64_t cols);
double sere_mat_det2(void* values);
double sere_mat_det3(void* values);
void* sere_mat4_identity(void);
void* sere_mat4_mul(void* left, void* right);
void* sere_mat4_transpose(void* values);
void* sere_mat4_inverse(void* values);
void* sere_mat4_translate(double x, double y, double z);
void* sere_mat4_scale(double x, double y, double z);
void* sere_mat4_rotate_x(double radians);
void* sere_mat4_rotate_y(double radians);
void* sere_mat4_rotate_z(double radians);
void* sere_mat4_perspective(double fov_y, double aspect, double near_z, double far_z);
void* sere_mat4_ortho(
    double left, double right, double bottom, double top, double near_z, double far_z);
void* sere_mat4_look_at(void* eye, void* center, void* up);
void* sere_mat4_transform_point(void* matrix, double x, double y, double z);
void* sere_mat4_transform_dir(void* matrix, double x, double y, double z);

void* sere_ml_relu(void* values);
void* sere_ml_leaky_relu(void* values, double alpha);
void* sere_ml_sigmoid(void* values);
void* sere_ml_tanh(void* values);
void* sere_ml_softmax(void* values);
void* sere_ml_log_softmax(void* values);
double sere_ml_mse(void* pred, void* target);
double sere_ml_mae(void* pred, void* target);
double sere_ml_bce(void* pred, void* target);
double sere_ml_linear(void* inputs, void* weights, double bias);

void* sere_bytes_alloc(int64_t length);
void* sere_bytes_copy(void* buffer);
void* sere_bytes_concat(void* left, void* right);
void sere_bytes_fill(void* buffer, uint8_t value);
void sere_bytes_set(void* buffer, int64_t index, uint8_t value);
uint8_t sere_bytes_get(void* buffer, int64_t index);
int32_t sere_bytes_eq(void* left, void* right);
int64_t sere_bytes_find(void* hay, void* needle);
void* sere_bytes_from_str(const char* data, int64_t len);
void sere_bytes_to_str(void* buffer, const char** out_data, int64_t* out_len);
void sere_bytes_hex(void* buffer, const char** out_data, int64_t* out_len);
void* sere_bytes_from_hex(const char* data, int64_t len);
void* sere_bytes_xor(void* left, void* right);
void* sere_bytes_and(void* left, void* right);
void* sere_bytes_or(void* left, void* right);
void* sere_bytes_not(void* buffer);
uint16_t sere_bytes_read_u16_le(void* buffer, int64_t index);
uint32_t sere_bytes_read_u32_le(void* buffer, int64_t index);
uint64_t sere_bytes_read_u64_le(void* buffer, int64_t index);
uint16_t sere_bytes_read_u16_be(void* buffer, int64_t index);
uint32_t sere_bytes_read_u32_be(void* buffer, int64_t index);
uint64_t sere_bytes_read_u64_be(void* buffer, int64_t index);
double sere_bytes_read_f32_le(void* buffer, int64_t index);
double sere_bytes_read_f64_le(void* buffer, int64_t index);
void sere_bytes_write_u16_le(void* buffer, int64_t index, uint16_t value);
void sere_bytes_write_u32_le(void* buffer, int64_t index, uint32_t value);
void sere_bytes_write_u64_le(void* buffer, int64_t index, uint64_t value);
void sere_bytes_write_u16_be(void* buffer, int64_t index, uint16_t value);
void sere_bytes_write_u32_be(void* buffer, int64_t index, uint32_t value);
void sere_bytes_write_u64_be(void* buffer, int64_t index, uint64_t value);
void sere_bytes_write_f32_le(void* buffer, int64_t index, double value);
void sere_bytes_write_f64_le(void* buffer, int64_t index, double value);

void sere_random_seed(int64_t seed);
int32_t sere_random_i32(void);
int64_t sere_random_i64(void);
double sere_random_f64(void);
int32_t sere_random_range(int32_t low, int32_t high);

int64_t sere_hash_fnv1a(const char* data, int64_t len);
int64_t sere_hash_combine(int64_t left, int64_t right);

void sere_sys_platform(const char** out_data, int64_t* out_len);
void sere_sys_arch(const char** out_data, int64_t* out_len);
void sere_sys_version(const char** out_data, int64_t* out_len);
int32_t sere_sys_pid(void);
int32_t sere_sys_cpu_count(void);

void sere_b64_encode(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_b64_decode(const char* data, int64_t len, const char** out_data, int64_t* out_len);

int64_t sere_string_find(const char* data, int64_t len, const char* needle, int64_t needle_len);
void sere_string_replace(const char* data,
                         int64_t len,
                         const char* old_data,
                         int64_t old_len,
                         const char* new_data,
                         int64_t new_len,
                         const char** out_data,
                         int64_t* out_len);
void* sere_string_split(const char* data, int64_t len, const char* sep, int64_t sep_len);
void sere_string_join(
    const char* sep, int64_t sep_len, void* parts, const char** out_data, int64_t* out_len);
int64_t sere_string_rfind(const char* data, int64_t len, const char* needle, int64_t needle_len);
int64_t sere_string_count(const char* data, int64_t len, const char* needle, int64_t needle_len);
void sere_string_capitalize(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_string_title(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_string_lstrip(const char* data, int64_t len, const char** out_data, int64_t* out_len);
void sere_string_rstrip(const char* data, int64_t len, const char** out_data, int64_t* out_len);
int32_t sere_string_is_empty(const char* data, int64_t len);
int32_t sere_string_is_digit(const char* data, int64_t len);
int32_t sere_string_is_alpha(const char* data, int64_t len);
int32_t sere_string_is_space(const char* data, int64_t len);

int32_t sere_win_available(void);
int32_t sere_win_message_box(
    const char* text, int64_t text_len, const char* title, int64_t title_len, int32_t flags);
int32_t sere_win_beep(int32_t freq, int32_t ms);
int32_t sere_win_last_error(void);
void sere_win_computer_name(const char** out_data, int64_t* out_len);
void sere_win_user_name(const char** out_data, int64_t* out_len);
int32_t sere_win_cursor_x(void);
int32_t sere_win_cursor_y(void);
int32_t sere_win_set_cursor(int32_t x, int32_t y);
int32_t sere_win_screen_width(void);
int32_t sere_win_screen_height(void);
void sere_win_clipboard_get(const char** out_data, int64_t* out_len);
int32_t sere_win_clipboard_set(const char* data, int64_t len);
void* sere_win_foreground(void);
void sere_win_window_text(void* hwnd, const char** out_data, int64_t* out_len);
void sere_win_debug(const char* data, int64_t len);
int32_t sere_win_open(const char* path, int64_t path_len);

int32_t sere_gl_available(void);
void* sere_gl_window_new(const char* title, int64_t title_len, int32_t width, int32_t height);
void* sere_gl_window_new_ex(
    const char* title, int64_t title_len, int32_t width, int32_t height, int32_t flags);
int32_t sere_gl_window_poll(void* window);
int32_t sere_gl_window_wait(void* window);
int32_t sere_gl_window_should_close(void* window);
void sere_gl_window_set_should_close(void* window, int32_t value);
int32_t sere_gl_window_is_open(void* window);
void sere_gl_window_swap(void* window);
void sere_gl_window_make_current(void* window);
void sere_gl_window_destroy(void* window);
int32_t sere_gl_window_width(void* window);
int32_t sere_gl_window_height(void* window);
int32_t sere_gl_window_fb_width(void* window);
int32_t sere_gl_window_fb_height(void* window);
int32_t sere_gl_window_x(void* window);
int32_t sere_gl_window_y(void* window);
void sere_gl_window_set_title(void* window, const char* title, int64_t title_len);
void sere_gl_window_title(void* window, const char** out_data, int64_t* out_len);
void sere_gl_window_set_vsync(void* window, int32_t enabled);
int32_t sere_gl_window_vsync(void* window);
void sere_gl_window_show_cursor(void* window, int32_t show);
void sere_gl_window_set_cursor_mode(void* window, int32_t mode);
int32_t sere_gl_window_cursor_mode(void* window);
void sere_gl_window_set_cursor_pos(void* window, int32_t x, int32_t y);
void sere_gl_window_set_size(void* window, int32_t width, int32_t height);
void sere_gl_window_set_pos(void* window, int32_t x, int32_t y);
void sere_gl_window_set_min_size(void* window, int32_t width, int32_t height);
void sere_gl_window_set_max_size(void* window, int32_t width, int32_t height);
void sere_gl_window_show(void* window);
void sere_gl_window_hide(void* window);
void sere_gl_window_minimize(void* window);
void sere_gl_window_maximize(void* window);
void sere_gl_window_restore(void* window);
void sere_gl_window_focus(void* window);
void sere_gl_window_request_attention(void* window);
void sere_gl_window_set_fullscreen(void* window, int32_t enabled);
int32_t sere_gl_window_fullscreen(void* window);
void sere_gl_window_set_resizable(void* window, int32_t enabled);
void sere_gl_window_set_decorated(void* window, int32_t enabled);
void sere_gl_window_set_floating(void* window, int32_t enabled);
void sere_gl_window_set_opacity(void* window, double opacity);
int32_t sere_gl_window_resizable(void* window);
int32_t sere_gl_window_decorated(void* window);
int32_t sere_gl_window_floating(void* window);
double sere_gl_window_opacity(void* window);
double sere_gl_window_content_scale(void* window);
int32_t sere_gl_window_focused(void* window);
int32_t sere_gl_window_minimized(void* window);
int32_t sere_gl_window_maximized(void* window);
int32_t sere_gl_window_visible(void* window);
int32_t sere_gl_window_hovered(void* window);
int32_t sere_gl_window_resized(void* window);
double sere_gl_window_time(void* window);
double sere_gl_window_dt(void* window);
int32_t sere_gl_window_key(void* window, int32_t vk);
int32_t sere_gl_window_key_pressed(void* window, int32_t vk);
int32_t sere_gl_window_key_released(void* window, int32_t vk);
int32_t sere_gl_window_mods(void* window);
int32_t sere_gl_window_char(void* window);
int32_t sere_gl_window_mouse_x(void* window);
int32_t sere_gl_window_mouse_y(void* window);
int32_t sere_gl_window_mouse_dx(void* window);
int32_t sere_gl_window_mouse_dy(void* window);
int32_t sere_gl_window_mouse_button(void* window, int32_t button);
int32_t sere_gl_window_mouse_button_pressed(void* window, int32_t button);
int32_t sere_gl_window_mouse_button_released(void* window, int32_t button);
int32_t sere_gl_window_wheel(void* window);
void sere_gl_window_clipboard_get(void* window, const char** out_data, int64_t* out_len);
void sere_gl_window_clipboard_set(void* window, const char* data, int64_t len);
int32_t sere_gl_screen_width(void);
int32_t sere_gl_screen_height(void);
void sere_gl_clear_color(double r, double g, double b, double a);
void sere_gl_clear(uint32_t mask);
void sere_gl_viewport(int32_t x, int32_t y, int32_t width, int32_t height);
void sere_gl_scissor(int32_t x, int32_t y, int32_t width, int32_t height);
void sere_gl_enable(uint32_t cap);
void sere_gl_disable(uint32_t cap);
void sere_gl_blend_func(uint32_t src, uint32_t dst);
void sere_gl_blend_func_separate(uint32_t src_rgb,
                                 uint32_t dst_rgb,
                                 uint32_t src_a,
                                 uint32_t dst_a);
void sere_gl_blend_equation(uint32_t mode);
void sere_gl_blend_equation_separate(uint32_t rgb, uint32_t alpha);
void sere_gl_blend_color(double r, double g, double b, double a);
void sere_gl_depth_func(uint32_t func);
void sere_gl_depth_mask(int32_t enabled);
void sere_gl_depth_range(double near_z, double far_z);
void sere_gl_color_mask(int32_t r, int32_t g, int32_t b, int32_t a);
void sere_gl_cull_face(uint32_t mode);
void sere_gl_front_face(uint32_t mode);
void sere_gl_polygon_mode(uint32_t face, uint32_t mode);
void sere_gl_polygon_offset(double factor, double units);
void sere_gl_line_width(double width);
void sere_gl_point_size(double size);
void sere_gl_pixel_store(uint32_t pname, int32_t value);
void sere_gl_hint(uint32_t target, uint32_t mode);
int32_t sere_gl_is_enabled(uint32_t cap);
void sere_gl_logic_op(uint32_t op);
void sere_gl_stencil_func(uint32_t func, int32_t ref, uint32_t mask);
void sere_gl_stencil_op(uint32_t sfail, uint32_t dpfail, uint32_t dppass);
void sere_gl_stencil_mask(uint32_t mask);
void sere_gl_clear_depth(double depth);
void sere_gl_clear_stencil(int32_t s);
void sere_gl_draw_buffer(uint32_t buf);
void sere_gl_read_buffer(uint32_t buf);
void sere_gl_sample_coverage(double value, int32_t invert);
double sere_gl_get_float(uint32_t pname);
int32_t sere_gl_validate_program(uint32_t program);
void sere_gl_finish(void);
void sere_gl_flush(void);
void sere_gl_begin(uint32_t mode);
void sere_gl_end(void);
void sere_gl_vertex2f(double x, double y);
void sere_gl_vertex3f(double x, double y, double z);
void sere_gl_color3f(double r, double g, double b);
void sere_gl_color4f(double r, double g, double b, double a);
void sere_gl_texcoord2f(double u, double v);
void sere_gl_normal3f(double x, double y, double z);
uint32_t sere_gl_get_error(void);
int32_t sere_gl_get_integer(uint32_t pname);
void sere_gl_get_string(uint32_t name, const char** out_data, int64_t* out_len);
uint32_t sere_gl_create_shader(uint32_t kind);
void sere_gl_shader_source(uint32_t shader, const char* src, int64_t src_len);
int32_t sere_gl_compile_shader(uint32_t shader);
void sere_gl_shader_log(uint32_t shader, const char** out_data, int64_t* out_len);
void sere_gl_delete_shader(uint32_t shader);
uint32_t sere_gl_create_program(void);
void sere_gl_attach_shader(uint32_t program, uint32_t shader);
void sere_gl_detach_shader(uint32_t program, uint32_t shader);
int32_t sere_gl_link_program(uint32_t program);
void sere_gl_program_log(uint32_t program, const char** out_data, int64_t* out_len);
void sere_gl_use_program(uint32_t program);
void sere_gl_delete_program(uint32_t program);
int32_t sere_gl_uniform_location(uint32_t program, const char* name, int64_t name_len);
int32_t sere_gl_attrib_location(uint32_t program, const char* name, int64_t name_len);
void sere_gl_bind_attrib(uint32_t program, uint32_t index, const char* name, int64_t name_len);
void sere_gl_uniform1f(int32_t location, double x);
void sere_gl_uniform2f(int32_t location, double x, double y);
void sere_gl_uniform3f(int32_t location, double x, double y, double z);
void sere_gl_uniform4f(int32_t location, double x, double y, double z, double w);
void sere_gl_uniform1i(int32_t location, int32_t x);
void sere_gl_uniform2i(int32_t location, int32_t x, int32_t y);
void sere_gl_uniform3i(int32_t location, int32_t x, int32_t y, int32_t z);
void sere_gl_uniform4i(int32_t location, int32_t x, int32_t y, int32_t z, int32_t w);
void sere_gl_uniform_vec(int32_t location, void* values);
void sere_gl_uniform_mat3(int32_t location, void* values);
void sere_gl_uniform_mat4(int32_t location, void* values);
uint32_t sere_gl_gen_buffer(void);
void sere_gl_delete_buffer(uint32_t buffer);
void sere_gl_bind_buffer(uint32_t target, uint32_t buffer);
void sere_gl_buffer_data_f64(uint32_t target, void* values, uint32_t usage);
void sere_gl_buffer_data_i32(uint32_t target, void* values, uint32_t usage);
void sere_gl_buffer_data_bytes(uint32_t target, void* values, uint32_t usage);
void sere_gl_buffer_sub_f64(uint32_t target, int64_t offset_bytes, void* values);
uint32_t sere_gl_gen_vao(void);
void sere_gl_delete_vao(uint32_t vao);
void sere_gl_bind_vao(uint32_t vao);
void sere_gl_enable_attrib(uint32_t index);
void sere_gl_disable_attrib(uint32_t index);
void sere_gl_attrib_pointer(uint32_t index,
                            int32_t size,
                            uint32_t type,
                            int32_t normalized,
                            int32_t stride,
                            int64_t offset);
void sere_gl_draw_arrays(uint32_t mode, int32_t first, int32_t count);
void sere_gl_draw_elements(uint32_t mode, int32_t count, uint32_t type, int64_t offset);
uint32_t sere_gl_gen_texture(void);
void sere_gl_delete_texture(uint32_t texture);
void sere_gl_bind_texture(uint32_t target, uint32_t texture);
void sere_gl_active_texture(uint32_t unit);
void sere_gl_tex_param(uint32_t target, uint32_t pname, int32_t value);
void sere_gl_tex_image2d(uint32_t target,
                         int32_t level,
                         int32_t internal,
                         int32_t width,
                         int32_t height,
                         uint32_t format,
                         uint32_t type,
                         void* pixels);
void sere_gl_tex_storage(
    uint32_t target, int32_t internal, int32_t width, int32_t height, uint32_t format);
void sere_gl_tex_sub_image2d(uint32_t target,
                             int32_t level,
                             int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height,
                             uint32_t format,
                             uint32_t type,
                             void* pixels);
void sere_gl_generate_mipmap(uint32_t target);
uint32_t sere_gl_gen_framebuffer(void);
void sere_gl_delete_framebuffer(uint32_t fbo);
void sere_gl_bind_framebuffer(uint32_t target, uint32_t fbo);
void sere_gl_framebuffer_texture2d(
    uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int32_t level);
uint32_t sere_gl_check_framebuffer(uint32_t target);
uint32_t sere_gl_gen_renderbuffer(void);
void sere_gl_delete_renderbuffer(uint32_t rbo);
void sere_gl_bind_renderbuffer(uint32_t target, uint32_t rbo);
void sere_gl_renderbuffer_storage(uint32_t target,
                                  uint32_t internal,
                                  int32_t width,
                                  int32_t height);
void sere_gl_framebuffer_renderbuffer(uint32_t target,
                                      uint32_t attachment,
                                      uint32_t rbo_target,
                                      uint32_t rbo);
void sere_gl_blit_framebuffer(int32_t src_x0,
                              int32_t src_y0,
                              int32_t src_x1,
                              int32_t src_y1,
                              int32_t dst_x0,
                              int32_t dst_y0,
                              int32_t dst_x1,
                              int32_t dst_y1,
                              uint32_t mask,
                              uint32_t filter);
void* sere_gl_read_pixels(
    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format, uint32_t type);

void sere_http_request(const char* method,
                       int64_t method_len,
                       const char* url,
                       int64_t url_len,
                       const char* body,
                       int64_t body_len,
                       int32_t timeout_ms,
                       const char** out_text,
                       int64_t* out_text_len);
int32_t sere_http_last_status(void);
void* sere_http_listen(const char* host, int64_t host_len, int32_t port);
void* sere_http_accept(void* server);
void sere_http_req_method(void* req, const char** out_data, int64_t* out_len);
void sere_http_req_path(void* req, const char** out_data, int64_t* out_len);
void sere_http_req_body(void* req, const char** out_data, int64_t* out_len);
void sere_http_reply(void* req,
                     int32_t status,
                     const char* content_type,
                     int64_t content_type_len,
                     const char* body,
                     int64_t body_len);
void sere_http_close(void* server);

#ifdef __cplusplus
}
#endif
