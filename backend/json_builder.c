#include "json_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void json_init(JsonBuilder* b, size_t initial_cap) {
    if (initial_cap < 512) initial_cap = 512;
    b->data = (char*)malloc(initial_cap);
    if (b->data) {
        b->data[0] = '\0';
    }
    b->length = 0;
    b->capacity = initial_cap;
}

void json_free(JsonBuilder* b) {
    if (b->data) {
        free(b->data);
        b->data = NULL;
    }
    b->length = 0;
    b->capacity = 0;
}

static void json_grow(JsonBuilder* b, size_t needed) {
    if (b->length + needed + 1 > b->capacity) {
        size_t new_cap = b->capacity * 2;
        if (new_cap < b->length + needed + 1024) {
            new_cap = b->length + needed + 1024;
        }
        char* new_data = (char*)realloc(b->data, new_cap);
        if (new_data) {
            b->data = new_data;
            b->capacity = new_cap;
        }
    }
}

void json_raw(JsonBuilder* b, const char* str) {
    if (!str || !b->data) return;
    size_t len = strlen(str);
    json_grow(b, len);
    memcpy(b->data + b->length, str, len);
    b->length += len;
    b->data[b->length] = '\0';
}

void json_printf(JsonBuilder* b, const char* fmt, ...) {
    if (!fmt || !b->data) return;
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed > 0) {
        json_grow(b, (size_t)needed);
        vsnprintf(b->data + b->length, (size_t)needed + 1, fmt, args);
        b->length += (size_t)needed;
    }
    va_end(args);
}

void json_escape(const char* src, char* dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0) return;
    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 2 < dst_size; si++) {
        unsigned char c = (unsigned char)src[si];
        if (c == '"') {
            if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '"'; }
        } else if (c == '\\') {
            if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = '\\'; }
        } else if (c == '\n') {
            if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'n'; }
        } else if (c == '\r') {
            if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 'r'; }
        } else if (c == '\t') {
            if (di + 2 < dst_size) { dst[di++] = '\\'; dst[di++] = 't'; }
        } else if (c >= 32 && c <= 126) {
            dst[di++] = (char)c;
        } else {
            dst[di++] = ' ';
        }
    }
    dst[di] = '\0';
}

void json_str(JsonBuilder* b, const char* key, const char* val) {
    char esc[1024];
    json_escape(val ? val : "", esc, sizeof(esc));
    json_printf(b, "\"%s\":\"%s\"", key, esc);
}

void json_int(JsonBuilder* b, const char* key, long long val) {
    json_printf(b, "\"%s\":%lld", key, val);
}

void json_double(JsonBuilder* b, const char* key, double val, int precision) {
    char fmt[32];
    snprintf(fmt, sizeof(fmt), "\"%%s\":%%.%df", precision);
    json_printf(b, fmt, key, val);
}

void json_bool(JsonBuilder* b, const char* key, int val) {
    json_printf(b, "\"%s\":%s", key, val ? "true" : "false");
}
