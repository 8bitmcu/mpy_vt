/*
 * MicroPython XML Interface Library
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 */

#include "py/obj.h"
#include "py/objlist.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include <stdio.h>
#include <string.h>

// Define the object structure missing from the original file
typedef struct _xml_obj_t {
  mp_obj_base_t base;
} xml_obj_t;

// Constructor
static mp_obj_t xml_make_new(const mp_obj_type_t *type, size_t n_args,
                             size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 0, 0, false);

  xml_obj_t *self = m_new_obj(xml_obj_t);
  self->base.type = type;

  return MP_OBJ_FROM_PTR(self);
}

// C Helper: Finds a tag, handles basic attributes, and returns the inner
// content string
static const char *get_tag_content(const char *xml, const char *tag,
                                   const char **end_ptr) {
  char open_target[64];
  snprintf(open_target, sizeof(open_target), "<%s", tag);

  char close_target[64];
  snprintf(close_target, sizeof(close_target), "</%s>", tag);

  const char *p = xml;
  while ((p = strstr(p, open_target)) != NULL) {
    // Ensure it's an exact tag match or has attributes (e.g. <item> or <item
    // id="1">)
    char next = p[strlen(open_target)];
    if (next == '>' || next == ' ' || next == '\n' || next == '\r' ||
        next == '\t') {

      // Find the end of the opening tag bracket
      const char *content_start = strchr(p, '>');
      if (!content_start)
        return NULL;
      content_start++; // Move past '>'

      // Find the closing tag
      const char *content_end = strstr(content_start, close_target);
      if (!content_end)
        return NULL;

      *end_ptr = content_end;
      return content_start;
    }
    p++; // Skip partial match (like finding <items> when looking for <item>)
  }
  return NULL;
}

// xml.find(xml_string, tag_name) -> Returns first match as a string
static mp_obj_t xml_find(mp_obj_t self_in, mp_obj_t xml_str_in,
                         mp_obj_t tag_in) {
  size_t xml_len, tag_len;
  const char *xml = mp_obj_str_get_data(xml_str_in, &xml_len);
  const char *tag = mp_obj_str_get_data(tag_in, &tag_len);

  const char *end_ptr = NULL;
  const char *start = get_tag_content(xml, tag, &end_ptr);

  if (start && end_ptr) {
    return mp_obj_new_str(start, end_ptr - start);
  }
  return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_3(xml_find_obj, xml_find);

// xml.findall(xml_string, tag_name) -> Returns a Python list of strings
static mp_obj_t xml_findall(mp_obj_t self_in, mp_obj_t xml_str_in,
                            mp_obj_t tag_in) {
  size_t xml_len, tag_len;
  const char *xml = mp_obj_str_get_data(xml_str_in, &xml_len);
  const char *tag = mp_obj_str_get_data(tag_in, &tag_len);

  mp_obj_t list = mp_obj_new_list(0, NULL);

  const char *p = xml;
  while (p != NULL) {
    const char *end_ptr = NULL;
    const char *start = get_tag_content(p, tag, &end_ptr);

    if (start && end_ptr) {
      // Append the extracted string to the Python list
      mp_obj_list_append(list, mp_obj_new_str(start, end_ptr - start));

      // Move pointer past the closing tag to search for the next one
      p = end_ptr + strlen(tag) + 3;
    } else {
      break;
    }
  }

  return list;
}

static MP_DEFINE_CONST_FUN_OBJ_3(xml_findall_obj, xml_findall);

// Register methods in the class dictionary
static const mp_rom_map_elem_t xml_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_find), MP_ROM_PTR(&xml_find_obj)},
    {MP_ROM_QSTR(MP_QSTR_findall), MP_ROM_PTR(&xml_findall_obj)},
};
static MP_DEFINE_CONST_DICT(xml_locals_dict, xml_locals_dict_table);

// Define the class type
MP_DEFINE_CONST_OBJ_TYPE(xml_type, MP_QSTR_XML, MP_TYPE_FLAG_NONE, make_new,
                         xml_make_new, locals_dict, &xml_locals_dict);

// Register the module
static const mp_rom_map_elem_t xml_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_XML)},
    {MP_ROM_QSTR(MP_QSTR_XML), MP_ROM_PTR(&xml_type)},
};
static MP_DEFINE_CONST_DICT(xml_module_globals, xml_module_globals_table);

const mp_obj_module_t xml_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&xml_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_XML, xml_user_cmodule);
