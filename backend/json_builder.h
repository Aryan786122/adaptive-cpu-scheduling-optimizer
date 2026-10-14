#pragma once
#include <windows.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} JsonBuilder;

void json_init(JsonBuilder* b, size_t initial_cap);
void json_free(JsonBuilder* b);
void json_raw(JsonBuilder* b, const char* str);
void json_printf(JsonBuilder* b, const char* fmt, ...);
void json_str(JsonBuilder* b, const char* key, const char* val);
void json_int(JsonBuilder* b, const char* key, long long val);
void json_double(JsonBuilder* b, const char* key, double val, int precision);
void json_bool(JsonBuilder* b, const char* key, int val);
void json_escape(const char* src, char* dst, size_t dst_size);

#ifdef __cplusplus
}
#endif
