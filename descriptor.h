#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <string>

using json = nlohmann::json;
using namespace std;

// Функція для перетворення JSON-таблиці (масиву об'єктів) у вектор рядків.
vector<map<string, json>> parseTable(const json& tableData);

// Функція для розбору всієї бази даних з JSON-об'єкта.
map<string, vector<map<string, json>>> parseDatabase(const json& db);

// Функція GetDS: аналізує таблицю та повертає дескрипторний набір з метаданими стовпців.
json GetDS(const string& tableName, const map<string, vector<map<string, json>>>& tables);

// Функція для виведення дескрипторного набору в зручному форматі.
void displayDescriptorSet(const string& tableName, const json& descriptorSet);

#endif // DESCRIPTOR_H
