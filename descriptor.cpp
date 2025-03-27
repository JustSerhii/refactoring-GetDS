#include "descriptor.h"
#include "utils.h"  
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

// Структура для проміжних даних дескриптора для кожного стовпця.
struct Descriptor {
    string type;
    int nonNullCount = 0;
    int maxLength = 0;    // для рядкових стовпців
    double min = 0;       // для числових стовпців
    double max = 0;       // для числових стовпців
    bool firstNumeric = true;
    set<string> distinct;
};

// Допоміжна функція: оновлення статистики дескриптора за значенням.
void updateDescriptor(Descriptor &d, const json &value) {
    if (!value.is_null()) {
        d.nonNullCount++;
        // Використовуємо value.dump() як рядкове представлення для зберігання у set.
        d.distinct.insert(value.dump());

        string currentType = getJsonType(value);
        // Якщо тип колонки ще не "Mixed" і зустрівся інший тип, робимо "Mixed".
        if (d.type != currentType) {
            d.type = (d.type.empty() ? currentType : "Mixed");
        }

        // Якщо це число - оновлюємо min/max.
        if (currentType == "Number") {
            double num = value.get<double>();
            if (d.firstNumeric) {
                d.min = num;
                d.max = num;
                d.firstNumeric = false;
            } else {
                if (num < d.min) d.min = num;
                if (num > d.max) d.max = num;
            }
        } 
        // Якщо це рядок - перевіряємо maxLength.
        else if (currentType == "String") {
            string s = value.get<string>();
            if (s.length() > (size_t)d.maxLength) {
                d.maxLength = s.length();
            }
        }
    } else {
        // Значення null (для distinct — збережемо "null")
        d.distinct.insert("null");
    }
}

json buildColumnDescriptor(const Descriptor &d, int totalRows) {
    json colDesc;
    colDesc["type"] = d.type.empty() ? "Unknown" : d.type;
    colDesc["nonNullCount"] = d.nonNullCount;
    colDesc["distinctCount"] = (int)d.distinct.size();

    // Визначення candidate key (ОДИН стовпець): 
    // якщо distinctCount == totalRows і не було null
    colDesc["candidateKey"] = (
        ((int)d.distinct.size() == totalRows) 
        && (d.nonNullCount == totalRows)
    );

    // Якщо тип "Number" (не "Mixed") - додаємо min/max
    if (colDesc["type"] == "Number") {
        colDesc["min"] = d.min;
        colDesc["max"] = d.max;
    }
    // Якщо тип "String" (не "Mixed") - додаємо maxLength
    if (colDesc["type"] == "String") {
        colDesc["maxLength"] = d.maxLength;
    }
    return colDesc;
}

vector<map<string, json>> parseTable(const json& tableData) {
    vector<map<string, json>> table;
    if (!tableData.is_array()) {
        cerr << "Error: Expected an array for table" << endl;
        return table;
    }
    for (const auto &row : tableData) {
        if (!row.is_object()) {
            cerr << "Error: Expected object rows in table" << endl;
            continue;
        }
        map<string, json> rowData;
        for (auto it = row.begin(); it != row.end(); ++it) {
            rowData[it.key()] = it.value();
        }
        table.push_back(rowData);
    }
    return table;
}

map<string, vector<map<string, json>>> parseDatabase(const json& db) {
    map<string, vector<map<string, json>>> tables;
    for (auto it = db.begin(); it != db.end(); ++it) {
        tables[it.key()] = parseTable(it.value());
    }
    return tables;
}

// згенерувати всі підмножини від розміру 2..N (для N=кількість стовпців)
static vector<vector<string>> generateColumnSubsets(const vector<string>& columns) {
    vector<vector<string>> allSubsets;
    int n = (int)columns.size();
    // Використаємо підхід з бітмасками: від 1 до (2^n - 1).
    // Але пропускаємо підмножини розміру 1 та 0.
    // (хоча 0 і так пропустимо, якщо почнемо з 1)
    int maxMask = (1 << n) - 1; // 2^n - 1
    for (int mask = 1; mask <= maxMask; ++mask) {
        // зберемо елементи, що входять у підмножину
        vector<string> subset;
        for (int bit = 0; bit < n; ++bit) {
            if (mask & (1 << bit)) {
                subset.push_back(columns[bit]);
            }
        }
        // Перевіримо розмір підмножини
        if (subset.size() >= 2) {
            allSubsets.push_back(subset);
        }
    }
    return allSubsets;
}

// Допоміжна функція: перевіряє, чи стовпці 'cols' разом утворюють candidate key
// (нема null та всі комбінації значень унікальні).
static bool isMultiColumnCandidateKey(const vector<map<string, json>>& table, 
                                      const vector<string>& cols) 
{
    if (table.empty() || cols.empty()) return false;

    set<string> combinations;
    for (const auto &row : table) {
        // Якщо хоч одне значення колонки == null, ключ не підходить
        ostringstream oss;
        bool hasNull = false;
        for (size_t i = 0; i < cols.size(); ++i) {
            auto it = row.find(cols[i]);
            if (it == row.end() || it->second.is_null()) {
                hasNull = true;
                break;
            }
            oss << it->second.dump();
            if (i < cols.size() - 1) {
                oss << "||";
            }
        }
        if (hasNull) {
            return false;
        }
        // Додаємо в set
        combinations.insert(oss.str());
    }
    // Якщо в set стільки ж елементів, скільки рядків, отже всі унікальні
    return (combinations.size() == table.size());
}

// Головна функція: аналізує таблицю та повертає дескриптори + мультиколонні ключі.

json GetDS(const string& tableName, const map<string, vector<map<string, json>>>& tables) {
    json descriptorSet;
    auto it = tables.find(tableName);
    if (it == tables.end()) {
        cerr << "Error: Table " << tableName << " not found" << endl;
        return descriptorSet; 
    }
    const auto &table = it->second;
    if (table.empty()) {
        cerr << "Error: Table " << tableName << " is empty" << endl;
        return descriptorSet; 
    }

    // Збираємо назви всіх стовпців (щоб знати, з чого утворювати підмножини).
    map<string, Descriptor> desc;
    set<string> colNames; 
    // Проходимо по всіх рядках:
    for (const auto &row : table) {
        // зберігаємо імена стовпців
        for (const auto &kv : row) {
            colNames.insert(kv.first);
        }
    }
    // Ініціалізуємо Descriptor для кожного стовпця
    for (auto &col : colNames) {
        desc[col] = Descriptor{ /*type=*/"", /*nonNullCount=*/0 };
    }

    // Оновлюємо Descriptor для кожного рядка
    for (const auto &row : table) {
        for (auto &col : colNames) {
            // Якщо в рядку немає такої колонки - вважаємо, що null
            auto itVal = row.find(col);
            if (itVal == row.end()) {
                updateDescriptor(desc[col], json(nullptr));
            } else {
                updateDescriptor(desc[col], itVal->second);
            }
        }
    }

    // Будуємо JSON-опис кожного стовпця
    int totalRows = (int)table.size();
    for (auto &col : colNames) {
        descriptorSet[col] = buildColumnDescriptor(desc[col], totalRows);
    }

    // Шукаємо мультиколонні ключі (2..N)
    vector<string> allCols(colNames.begin(), colNames.end());
    vector<vector<string>> subsets = generateColumnSubsets(allCols);

    // Збережемо всі знайдені мультиколонні ключі у масив JSON
    json multiKeys = json::array();
    for (auto &subset : subsets) {
        if (isMultiColumnCandidateKey(table, subset)) {
            multiKeys.push_back(subset);
        }
    }

    descriptorSet["multiCandidateKeys"] = multiKeys;

    return descriptorSet;
}

// Функція виведення дескрипторного набору у консоль.
void displayDescriptorSet(const string& tableName, const json& descriptorSet) {
    cout << "Descriptor Set for table '" << tableName << "':" << endl;
    cout << "---------------------------------------" << endl;

    // Спочатку виведемо колонкові дескриптори
    for (auto it = descriptorSet.begin(); it != descriptorSet.end(); ++it) {
        if (it.key() == "multiCandidateKeys") {
            continue;
        }
        cout << "Column: " << it.key() << endl;
        for (auto innerIt = it.value().begin(); innerIt != it.value().end(); ++innerIt) {
            cout << "  " << innerIt.key() << ": " << innerIt.value() << endl;
        }
        cout << "---------------------------------------" << endl;
    }

    cout << "Multi-Column Candidate Keys:" << endl;
    auto mkIter = descriptorSet.find("multiCandidateKeys");
    if (mkIter != descriptorSet.end() && mkIter->is_array()) {
        auto arr = mkIter.value();
        if (arr.empty()) {
            cout << "  None" << endl;
        } else {
            for (auto &subset : arr) {
                // subset – це масив назв колонок
                cout << "  [ ";
                for (size_t i = 0; i < subset.size(); ++i) {
                    cout << subset[i].get<string>();
                    if (i + 1 < subset.size()) cout << ", ";
                }
                cout << " ]" << endl;
            }
        }
    }
    cout << "---------------------------------------" << endl;
}