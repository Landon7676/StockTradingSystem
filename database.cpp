#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include "database.h"
#include <iostream>

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

bool addUser(const std::string &first_name,
             const std::string &last_name,
             const std::string &user_name,
             const std::string &password,
             double balance)
{
    sqlite3 *db = nullptr;

    // Open the database
    int rc = sqlite3_open("trading.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Prepare the SQL statement
    const char *sql = R"(
        INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
        VALUES (?, ?, ?, ?, ?);
    )";
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error preparing statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind parameters
    rc = sqlite3_bind_text(stmt, 1, first_name.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 2, last_name.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 3, user_name.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(stmt, 4, password.c_str(), -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_double(stmt, 5, balance);

    if (rc != SQLITE_OK) {
        std::cerr << "Error binding parameters: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    // Execute
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Error executing insert: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    // Cleanup
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "User '" << user_name << "' added successfully.\n";
    return true;
}


void listUsers() {
    sqlite3 *db = nullptr;

    // Open the database
    int rc = sqlite3_open("trading.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT ID, first_name, last_name, user_name, usd_balance FROM Users;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing listUsers: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);  // Fix memory leak
        return;
    }

    std::cout << "ID | First Name | Last Name | Username | Balance\n";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << sqlite3_column_int(stmt, 0) << " | "
                  << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) << " | "
                  << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) << " | "
                  << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) << " | $"
                  << sqlite3_column_double(stmt, 4) << "\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db); // Close DB connection
}

double getUserBalance(int userId) {
    sqlite3 *db = nullptr;
    double balance = 0.0;

    // Open the database
    int rc = sqlite3_open("trading.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return 0.0;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT usd_balance FROM Users WHERE ID = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing getUserBalance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);  // Fix memory leak
        return 0.0;
    }

    sqlite3_bind_int(stmt, 1, userId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        balance = sqlite3_column_double(stmt, 0);
    } else {
        std::cerr << "User ID not found.\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db); // Close DB connection
    return balance;
}

bool updateUserBalance(int userId, double newBalance) {
    sqlite3 *db = nullptr;

    // Open the database
    int rc = sqlite3_open("trading.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE Users SET usd_balance = ? WHERE ID = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing updateUserBalance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);  // Fix memory leak
        return false;
    }

    sqlite3_bind_double(stmt, 1, newBalance);
    sqlite3_bind_int(stmt, 2, userId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Error updating balance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db); // Close DB connection
    return true;
}


#endif