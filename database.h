#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <sstream>


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

bool listStock(const std::string &dbName,
                 int user_id);

bool getUserBalance(int user_id, 
                    std::string &first_name,
                    std::string &last_name,
                    double &usd_balance,
                    const std::string &dbName);

bool checkCredentials(const std::string &userName, const std::string &password, int &user_id_out, const std::string &dbName);
bool depositAmount(int user_id, double amount, const std::string &dbName);

double getUserBalance(int user_id, const std::string &dbName);

std::string getActiveUsers();

std::string lookupStock(int user_id, const std::string &ticker, const std::string &dbName);

std::string listStocks(int user_id, bool isRoot, const std::string &dbName);

                    
#endif
