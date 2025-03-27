#include <iostream>
#include <fstream>
#include "descriptor.h"

using namespace std;
using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <database_name> <table_name>" << endl;
        return 1;
    }
    string database_name = argv[1];
    string tableName = argv[2];

    ifstream file(database_name);
    if (!file) {
        cerr << "Error: Could not open file " << database_name << endl;
        return 1;
    }
    json db;
    file >> db;
    file.close();

    // Розбираємо базу даних з JSON-файлу.
    auto tables = parseDatabase(db);

    // Отримуємо дескрипторний набір для заданої таблиці з урахуванням мультиколонних ключів.
    json descriptorSet = GetDS(tableName, tables);
    if (descriptorSet.empty()) {
        cerr << "No descriptor set found for table " << tableName << endl;
        return 1;
    }

    // Виводимо дескрипторний набір у зручному форматі.
    displayDescriptorSet(tableName, descriptorSet);
    
    return 0;
}
