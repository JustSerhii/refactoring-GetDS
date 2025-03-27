#ifndef UTILS_H
#define UTILS_H

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace std;

// Функція, яка повертає тип JSON-значення у вигляді рядка.
string getJsonType(const json& value);

#endif // UTILS_H
