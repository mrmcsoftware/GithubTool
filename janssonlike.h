#ifndef JANSSONLIKE_H
#define JANSSONLIKE_H

#define json_t yyjson_val
#define json_object_get yyjson_obj_get
#define json_string_value yyjson_get_str
#define json_integer_value yyjson_get_int
#define json_is_array yyjson_is_arr
//#define json_array_size yyjson_get_len
#define json_array_size yyjson_arr_size
#define json_is_object yyjson_is_obj
#define json_is_string yyjson_is_str
#define json_is_integer yyjson_is_int
#define json_is_true yyjson_is_true
#define json_array_get yyjson_arr_get
// BEWARE: yyjson's key isn't the same type as jansson's
#define json_object_foreach(obj,key,value) yyjson_obj_foreach(obj,_yyidx,_yymax,key,value)

#include "yyjson.c"

#endif
