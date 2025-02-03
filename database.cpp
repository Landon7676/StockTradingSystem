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
    sqlite3 *db = nullptr;
    if (!openDatabase(&db, dbName)) {
        return false;
    }

    char *errMsg = nullptr;

    // Create Users table
    const char *createUsersTable =
        "CREATE TABLE IF NOT EXISTS Users ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "first_name TEXT, "
        "last_name TEXT, "
        "user_name TEXT NOT NULL UNIQUE, "
        "password TEXT, "
        "usd_balance DOUBLE NOT NULL"
        ");";

    // Create Stocks table
    const char *createStocksTable =
        "CREATE TABLE IF NOT EXISTS Stocks ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stock_symbol TEXT NOT NULL, "
        "stock_name TEXT NOT NULL, "
        "stock_balance DOUBLE, "
        "user_id INTEGER, "
        "FOREIGN KEY (user_id) REFERENCES Users(ID) ON DELETE CASCADE"
        ");";

    // Execute table creation
    int rc = sqlite3_exec(db, createUsersTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating Users table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    rc = sqlite3_exec(db, createStocksTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error creating Stocks table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // Check if there is at least one user in the database
    const char *checkUserSQL = "SELECT COUNT(*) FROM Users;";
    sqlite3_stmt *stmt;
    int user_count = 0;

    rc = sqlite3_prepare_v2(db, checkUserSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Error checking user count: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // If no users exist, insert a default user
    if (user_count == 0) {
        std::cout << "No users found. Creating default user...\n";

        const char *insertDefaultUser = R"(
            INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
            VALUES ('John', 'Doe', 'admin', 'password', 100.00);
        )";

        rc = sqlite3_exec(db, insertDefaultUser, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "Error inserting default user: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            sqlite3_close(db);
            return false;
        }

        std::cout << "Default user created successfully. (Username: admin, Password: password, Balance: $100.00)\n";
    } else {
        std::cout << "User(s) found in the database. No default user needed.\n";
    }

    sqlite3_close(db);
    return true;
}


bool buyStock(const std::string &stock_symbol,
              const std::string &stock_name,
              double amount,
              double price_per_stock,
              int user_id,
              const std::string &dbName)
{
    sqlite3 *db;
    int rc;
    sqlite3_stmt* stmt;

    if (!openDatabase(&db, dbName)) {
        return false;
    }
    // Query to check if the user exists
    const char *userExists = "SELECT COUNT(*) FROM Users WHERE ID = ?;";

    // Prepare the SQL statement
    rc = sqlite3_prepare_v2(db, userExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind user_id to the query
    sqlite3_bind_int(stmt, 1, user_id);

    // Execute statement and check if user exists
    bool userExistsFlag = false;
    if (sqlite3_step(stmt) == SQLITE_ROW){
        userExistsFlag = sqlite3_column_int(stmt, 0) > 0; // Get the COUNT(*) value
    }
    
    // Finalize statement
    sqlite3_finalize(stmt);

    if(!userExistsFlag){
        std::cerr << "User ID " << user_id << " does not exist!" << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Query to check for stock
    const char *stockExists = "SELECT COUNT(*) FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";

    // Prepare statement
    rc = sqlite3_prepare_v2(db, stockExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind two values to query
    sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);

    // Execute the query
    rc = sqlite3_step(stmt);

    // Check if stock exists
    bool stockExistsFlag = false;
    int stockCount = 0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stockCount = sqlite3_column_int(stmt, 0);
    }

    stockExistsFlag = stockCount > 0;

    sqlite3_finalize(stmt);

    // If the stock does not exist then we insert a new one
    if (stockExistsFlag == false) {
        // Query to insert new stock
        const char *stockInsert = R"(
            INSERT INTO Stocks (stock_symbol, stock_name, stock_balance, user_id)
            VALUES (?, ?, ?, ?);
        )";

        // Prepare statement
        rc = sqlite3_prepare_v2(db, stockInsert, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare INSERT statement: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, stock_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, amount);
        sqlite3_bind_int(stmt, 4, user_id);

        // Execute the INSERT statement
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Error inserting new stock: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "New stock record inserted successfully!" << std::endl;
        }

        sqlite3_finalize(stmt);
    }

    double total_cost = amount * price_per_stock;

    // Query to check user balance
    const char *userBalance = "SELECT usd_balance FROM Users WHERE ID = ?;";

    rc = sqlite3_prepare_v2(db, userBalance, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare balance check statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind user ID
    sqlite3_bind_int(stmt, 1, user_id);

    // Execute query
    double usd_balance = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        usd_balance = sqlite3_column_double(stmt, 0);
    }

    sqlite3_finalize(stmt);
    if(usd_balance < total_cost){
        std::cerr << "User does not have enough funds! Balance: $ " << usd_balance << ", Required: " << total_cost << std::endl;
        sqlite3_close(db);
        return false;
    }

    const char *deductBalanceSQL = "UPDATE Users SET usd_balance = usd_balance - ? WHERE ID = ?;";

    rc = sqlite3_prepare_v2(db, deductBalanceSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare balance deduction statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind total cost and user ID
    sqlite3_bind_double(stmt, 1, total_cost);
    sqlite3_bind_int(stmt, 2, user_id);

    // Execute update
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Error updating user balance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;  
    }
    
    sqlite3_finalize(stmt);  
    sqlite3_close(db);
    return true;
}

bool sellStock(const std::string &stock_symbol,
               double amount,
               double price_per_stock,
               int user_id,
               const std::string &dbName)
{
    sqlite3 *db;
    int rc;
    sqlite3_stmt* stmt;

    if (!openDatabase(&db, dbName)) {
        return false;
    }

    // Start transaction
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Check if the user exists
    const char *userExists = "SELECT COUNT(*) FROM Users WHERE ID = ?;";
    rc = sqlite3_prepare_v2(db, userExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare user existence check: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    bool userExistsFlag = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userExistsFlag = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);

    if (!userExistsFlag) {
        std::cerr << "User ID " << user_id << " does not exist!" << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Check if the stock exists and get the current balance
    const char *stockExists = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
    rc = sqlite3_prepare_v2(db, stockExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare stock existence check: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);

    double stockBalance = 0.0;
    bool stockExistsFlag = false;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stockBalance = sqlite3_column_double(stmt, 0);
        stockExistsFlag = stockBalance >= amount; // Ensure sufficient stock
    }

    sqlite3_finalize(stmt);

    if (!stockExistsFlag) {
        std::cerr << "Insufficient stock balance to sell. Available: " << stockBalance << ", Attempted to sell: " << amount << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Update or delete the stock record
    if (stockBalance == amount) {
        const char *deleteStockSQL = "DELETE FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
        rc = sqlite3_prepare_v2(db, deleteStockSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare stock deletion: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, user_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Error deleting stock: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
    } else {
        const char *updateStockSQL = "UPDATE Stocks SET stock_balance = stock_balance - ? WHERE stock_symbol = ? AND user_id = ?;";
        rc = sqlite3_prepare_v2(db, updateStockSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare stock balance update: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_double(stmt, 1, amount);
        sqlite3_bind_text(stmt, 2, stock_symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, user_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            std::cerr << "Error updating stock balance: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
    }

    // Update user's USD balance
    double total_earnings = amount * price_per_stock;
    const char *updateBalanceSQL = "UPDATE Users SET usd_balance = usd_balance + ? WHERE ID = ?;";
    rc = sqlite3_prepare_v2(db, updateBalanceSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare balance update: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_double(stmt, 1, total_earnings);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Error updating user balance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Commit transaction so the sell actually works
    rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    return true;
}


#endif