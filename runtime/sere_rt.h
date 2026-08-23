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
const char* sere_str_concat_data(const char* left, int64_t left_len, const char* right,
                                int64_t right_len, int64_t* out_len);
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
void* sere_list_item(void* list, int64_t index);
void* sere_list_slice(void* list, int64_t start, int64_t end, int32_t has_start, int32_t has_end);
void* sere_list_from_argv(int argc, char** argv);
void* sere_dict_new(int64_t key_stride, int64_t val_stride, int32_t key_kind);
void sere_dict_set(void* dict, const void* key, const void* value);
int32_t sere_dict_get(void* dict, const void* key, void* out_value);
int64_t sere_dict_len(void* dict);

void sere_str_index(const char* data, int64_t len, int64_t index, const char** out_data,
                    int64_t* out_len);
void sere_str_slice(const char* data, int64_t len, int64_t start, int64_t end, int32_t has_start,
                    int32_t has_end, const char** out_data, int64_t* out_len);
int32_t sere_str_contains(const char* hay, int64_t hay_len, const char* needle, int64_t needle_len);
int32_t sere_str_eq(const char* left, int64_t left_len, const char* right, int64_t right_len);
int32_t sere_str_cmp(const char* left, int64_t left_len, const char* right, int64_t right_len);
void sere_str_repeat(const char* data, int64_t len, int64_t count, const char** out_data,
                     int64_t* out_len);
void sere_raise(const char* type, const char* message, int64_t message_len);
int32_t sere_has_error(void);
void sere_clear_error(void);
int32_t sere_error_isa(const char* name);
const char* sere_error_type(void);
const char* sere_error_message(int64_t* out_len);
void sere_panic(const char* message, int64_t len);

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

void sere_path_join(const char* left, int64_t left_len, const char* right, int64_t right_len,
                    const char** out_data, int64_t* out_len);
void sere_path_dirname(const char* path, int64_t path_len, const char** out_data, int64_t* out_len);
void sere_path_basename(const char* path, int64_t path_len, const char** out_data, int64_t* out_len);
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
int32_t sere_string_starts_with(const char* data, int64_t len, const char* prefix, int64_t prefix_len);
int32_t sere_string_ends_with(const char* data, int64_t len, const char* suffix, int64_t suffix_len);
void sere_string_repeat(const char* data, int64_t len, int64_t count, const char** out_data,
                        int64_t* out_len);

int32_t sere_re_is_match(const char* pattern, int64_t pattern_len, const char* text,
                         int64_t text_len);
void sere_re_find(const char* pattern, int64_t pattern_len, const char* text, int64_t text_len,
                  const char** out_data, int64_t* out_len);
void sere_re_replace(const char* pattern, int64_t pattern_len, const char* text, int64_t text_len,
                     const char* repl, int64_t repl_len, const char** out_data, int64_t* out_len);

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
void* sere_mat_mul(void* left, int64_t left_rows, int64_t left_cols, void* right, int64_t right_rows,
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
void* sere_mat4_ortho(double left, double right, double bottom, double top, double near_z,
                      double far_z);
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
void sere_string_replace(const char* data, int64_t len, const char* old_data, int64_t old_len,
                         const char* new_data, int64_t new_len, const char** out_data,
                         int64_t* out_len);
void* sere_string_split(const char* data, int64_t len, const char* sep, int64_t sep_len);

int32_t sere_win_available(void);
int32_t sere_win_message_box(const char* text, int64_t text_len, const char* title,
                             int64_t title_len, int32_t flags);
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
int32_t sere_gl_window_poll(void* window);
void sere_gl_window_swap(void* window);
void sere_gl_window_make_current(void* window);
void sere_gl_window_destroy(void* window);
int32_t sere_gl_window_width(void* window);
int32_t sere_gl_window_height(void* window);
void sere_gl_clear_color(float r, float g, float b, float a);
void sere_gl_clear(uint32_t mask);
void sere_gl_viewport(int32_t x, int32_t y, int32_t width, int32_t height);
void sere_gl_enable(uint32_t cap);
void sere_gl_disable(uint32_t cap);
void sere_gl_begin(uint32_t mode);
void sere_gl_end(void);
void sere_gl_vertex3f(float x, float y, float z);
void sere_gl_color3f(float r, float g, float b);
uint32_t sere_gl_get_error(void);
uint32_t sere_gl_create_shader(uint32_t kind);
void sere_gl_shader_source(uint32_t shader, const char* src, int64_t src_len);
int32_t sere_gl_compile_shader(uint32_t shader);
void sere_gl_shader_log(uint32_t shader, const char** out_data, int64_t* out_len);
void sere_gl_delete_shader(uint32_t shader);
uint32_t sere_gl_create_program(void);
void sere_gl_attach_shader(uint32_t program, uint32_t shader);
int32_t sere_gl_link_program(uint32_t program);
void sere_gl_program_log(uint32_t program, const char** out_data, int64_t* out_len);
void sere_gl_use_program(uint32_t program);
void sere_gl_delete_program(uint32_t program);

#ifdef __cplusplus
}
#endif
