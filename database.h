#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>

bool openDatabase(sqlite3 **db, const std::string &dbName);

bool initializeDatabase(const std::string &dbName);

bool buyStock(const std::string &stock_symbol,
              const std::string &stock_name,
              double amount,
              double price_per_stock,
              int user_id,
              const std::string &dbName);

bool sellStock(const std::string &stock_symbol,
               double amount,
               double price_per_stock,
               int user_id,
               const std::string &dbName);

#endif 
