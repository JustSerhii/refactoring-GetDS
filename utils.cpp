#include "utils.h"

string getJsonType(const json& value) {
    if (value.is_number())   return "Number";
    else if (value.is_string())  return "String";
    else if (value.is_boolean()) return "Boolean";
    else if (value.is_array())   return "Array";
    else if (value.is_object())  return "Object";
    else if (value.is_null())    return "Null";
    return "Unknown";
}
