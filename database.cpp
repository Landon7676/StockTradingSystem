#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include "database.h"

bool openDatabase(sqlite3 **db, const std::string &dbName) {
    int rc = sqlite3_open(dbName.c_str(), db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(*db) << std::endl;
        return false;
    }
    std::cout << "Database '" << dbName << "' opened successfully.\n";
    return true;
}

bool initializeDatabase(const std::string &dbName) {
    sqlite3 *db;
    
    if (!openDatabase(&db, dbName)) {
        return false;  // Database failed to open
    }

    char *errMsg = nullptr;

    // SQL query to create the Users table
    const char *createUsersTable =
        "CREATE TABLE IF NOT EXISTS Users ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "first_name TEXT, "
        "last_name TEXT, "
        "user_name TEXT NOT NULL UNIQUE, "
        "password TEXT, "
        "usd_balance DOUBLE NOT NULL"
        ");";

    // SQL query to create the Stocks table
    const char *createStocksTable =
        "CREATE TABLE IF NOT EXISTS Stocks ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stock_symbol TEXT NOT NULL, "
        "stock_name TEXT NOT NULL, "
        "stock_balance DOUBLE, "
        "user_id INTEGER, "
        "FOREIGN KEY (user_id) REFERENCES Users(ID) ON DELETE CASCADE"
        ");";

    // Execute the Users table query
    int rc = sqlite3_exec(db, createUsersTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating Users table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    std::cout << "Users table created successfully.\n";

    // Execute the Stocks table query
    rc = sqlite3_exec(db, createStocksTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating Stocks table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    std::cout << "Stocks table created successfully.\n";

    // Close the database
    sqlite3_close(db);
    return true;
}

bool addUser(const std::string &first_name, const std::string &last_name,
             const std::string &user_name, const std::string &password, double balance);
void listUsers();
double getUserBalance(int userId);
bool updateUserBalance(int userId, double newBalance);

#endif