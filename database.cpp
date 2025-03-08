#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include "database.h"
#include <iostream>
#include <sstream>


bool openDatabase(sqlite3 **db, const std::string &dbName)
{
    std::cout << "Opening database at path: " << dbName << std::endl;
    int rc = sqlite3_open(dbName.c_str(), db);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Error opening database: " << sqlite3_errmsg(*db) << std::endl;
        return false;
    }
    std::cout << "Database '" << dbName << "' opened successfully.\n";
    return true;
}
bool initializeDatabase(const std::string &dbName)
{
    sqlite3 *db = nullptr;
    if (!openDatabase(&db, dbName))
    {
        return false;
    }

    char *errMsg = nullptr;
    sqlite3_stmt *stmt;

    const char *createUsersTable =
        "CREATE TABLE IF NOT EXISTS Users ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "first_name TEXT, "
        "last_name TEXT, "
        "user_name TEXT NOT NULL UNIQUE, "
        "password TEXT, "
        "usd_balance DOUBLE NOT NULL"
        ");";

    const char *createStocksTable =
        "CREATE TABLE IF NOT EXISTS Stocks ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stock_symbol TEXT NOT NULL, "
        "stock_name TEXT NOT NULL, "
        "stock_balance DOUBLE, "
        "user_id INTEGER, "
        "FOREIGN KEY (user_id) REFERENCES Users(ID) ON DELETE CASCADE"
        ");";

    // Create Users table
    int rc = sqlite3_exec(db, createUsersTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Error creating Users table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // Create Stocks table
    rc = sqlite3_exec(db, createStocksTable, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Error creating Stocks table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // Check if there is at least one user
    const char *checkUserSQL = "SELECT COUNT(*) FROM Users;";
    rc = sqlite3_prepare_v2(db, checkUserSQL, -1, &stmt, nullptr);
    if (rc == SQLITE_OK)
    {
        int user_count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            user_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        
        if (user_count == 0)
        {
            std::cout << "No users found. Creating default user database..." << std::endl;
            const char *insertRootUser = R"(
                INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
                VALUES ('John', 'Doe', 'root', 'root1', 100.00);
            )";

            rc = sqlite3_exec(db, insertRootUser, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK)
            {
                std::cerr << "Error inserting user: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                sqlite3_close(db);
                return false;
            }

            const char *insertMaryUser = R"(
                INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
                VALUES ('Mary', 'Jane', 'mary', 'mary01', 100.00);
            )";

            rc = sqlite3_exec(db, insertMaryUser, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK)
            {
                std::cerr << "Error inserting user: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                sqlite3_close(db);
                return false;
            }

            const char *insertJohnUser = R"(
                INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
                VALUES ('John', 'Deere', 'john', 'john01', 100.00);
            )";

            rc = sqlite3_exec(db, insertJohnUser, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK)
            {
                std::cerr << "Error inserting user: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                sqlite3_close(db);
                return false;
            }

            const char *insertMoeUser = R"(
                INSERT INTO Users (first_name, last_name, user_name, password, usd_balance)
                VALUES ('Moe', 'Stizmak', 'moe', 'moe01', 100.00);
            )";

            rc = sqlite3_exec(db, insertMoeUser, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK)
            {
                std::cerr << "Error inserting user: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                sqlite3_close(db);
                return false;
            }

            std::cout << "Default user database created successfully." <<
            "\n(Username: root, Password: root1, Balance: $100.00)" <<
            "\n(Username: mary, Password: mary01, Balance: $100.00)" <<
            "\n(Username: john, Password: john01, Balance: $100.00)" <<
            "\n(Username: moe, Password: moe01, Balance: $100.00)" << std::endl;
        }
        else
        {
            std::cout << "User(s) found in the database. No default user database needed." << std::endl;
        }
    }
    else
    {
        std::cerr << "Error checking user count: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
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
sqlite3_stmt *stmt;

if (!openDatabase(&db, dbName))
{
return false;
}

double total_cost = amount * price_per_stock;

// Step 1: Check user's balance BEFORE making any changes
const char *userBalanceSQL = "SELECT usd_balance FROM Users WHERE ID = ?;";
rc = sqlite3_prepare_v2(db, userBalanceSQL, -1, &stmt, nullptr);
if (rc != SQLITE_OK)
{
std::cerr << "Failed to prepare balance check statement: " << sqlite3_errmsg(db) << std::endl;
sqlite3_close(db);
return false;
}

sqlite3_bind_int(stmt, 1, user_id);

double usd_balance = 0.0;
if (sqlite3_step(stmt) == SQLITE_ROW)
{
usd_balance = sqlite3_column_double(stmt, 0);
}

sqlite3_finalize(stmt);

if (usd_balance < total_cost)
{
std::cerr << "Insufficient funds! Balance: $" << usd_balance << ", Required: $" << total_cost << std::endl;
sqlite3_close(db);
return false; // Stop the function if not enough funds
}

// Step 2: Check if the stock already exists for the user
const char *stockExistsSQL = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
rc = sqlite3_prepare_v2(db, stockExistsSQL, -1, &stmt, nullptr);
if (rc != SQLITE_OK)
{
std::cerr << "Failed to prepare stock existence check: " << sqlite3_errmsg(db) << std::endl;
sqlite3_close(db);
return false;
}

sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_int(stmt, 2, user_id);

bool stockExists = false;
double existingStockBalance = 0.0;

if (sqlite3_step(stmt) == SQLITE_ROW)
{
stockExists = true;
existingStockBalance = sqlite3_column_double(stmt, 0);
}

sqlite3_finalize(stmt);

// Step 3: Deduct the balance
const char *deductBalanceSQL = "UPDATE Users SET usd_balance = usd_balance - ? WHERE ID = ?;";
rc = sqlite3_prepare_v2(db, deductBalanceSQL, -1, &stmt, nullptr);
if (rc != SQLITE_OK)
{
std::cerr << "Failed to prepare balance deduction statement: " << sqlite3_errmsg(db) << std::endl;
sqlite3_close(db);
return false;
}

sqlite3_bind_double(stmt, 1, total_cost);
sqlite3_bind_int(stmt, 2, user_id);

if (sqlite3_step(stmt) != SQLITE_DONE)
{
std::cerr << "Error deducting user balance: " << sqlite3_errmsg(db) << std::endl;
sqlite3_finalize(stmt);
sqlite3_close(db);
return false;
}

sqlite3_finalize(stmt);

// Step 4: Insert or update stock record
if (stockExists)
{
const char *updateStockSQL = "UPDATE Stocks SET stock_balance = stock_balance + ? WHERE stock_symbol = ? AND user_id = ?;";
rc = sqlite3_prepare_v2(db, updateStockSQL, -1, &stmt, nullptr);
if (rc != SQLITE_OK)
{
  std::cerr << "Failed to prepare stock update statement: " << sqlite3_errmsg(db) << std::endl;
  sqlite3_close(db);
  return false;
}

sqlite3_bind_double(stmt, 1, amount);
sqlite3_bind_text(stmt, 2, stock_symbol.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_int(stmt, 3, user_id);

if (sqlite3_step(stmt) != SQLITE_DONE)
{
  std::cerr << "Error updating stock balance: " << sqlite3_errmsg(db) << std::endl;
}

sqlite3_finalize(stmt);
}
else
{
const char *insertStockSQL = "INSERT INTO Stocks (stock_symbol, stock_name, stock_balance, user_id) VALUES (?, ?, ?, ?);";
rc = sqlite3_prepare_v2(db, insertStockSQL, -1, &stmt, nullptr);
if (rc != SQLITE_OK)
{
  std::cerr << "Failed to prepare stock insert statement: " << sqlite3_errmsg(db) << std::endl;
  sqlite3_close(db);
  return false;
}

sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_text(stmt, 2, stock_name.c_str(), -1, SQLITE_STATIC);
sqlite3_bind_double(stmt, 3, amount);
sqlite3_bind_int(stmt, 4, user_id);

if (sqlite3_step(stmt) != SQLITE_DONE)
{
  std::cerr << "Error inserting new stock: " << sqlite3_errmsg(db) << std::endl;
}

sqlite3_finalize(stmt);
}

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
    sqlite3_stmt *stmt;

    if (!openDatabase(&db, dbName))
    {
        return false;
    }

    // Start transaction
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Check if the user exists
    const char *userExists = "SELECT COUNT(*) FROM Users WHERE ID = ?;";
    rc = sqlite3_prepare_v2(db, userExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare user existence check: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    bool userExistsFlag = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        userExistsFlag = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);

    if (!userExistsFlag)
    {
        std::cerr << "User ID " << user_id << " does not exist!" << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Check if the stock exists and get the current balance
    const char *stockExists = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
    rc = sqlite3_prepare_v2(db, stockExists, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare stock existence check: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);

    double stockBalance = 0.0;
    bool stockExistsFlag = false;

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        stockBalance = sqlite3_column_double(stmt, 0);
        stockExistsFlag = stockBalance >= amount; // Ensure sufficient stock
    }

    sqlite3_finalize(stmt);

    if (!stockExistsFlag)
    {
        std::cerr << "Insufficient stock balance to sell. Available: " << stockBalance << ", Attempted to sell: " << amount << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Update or delete the stock record
    if (stockBalance == amount)
    {
        const char *deleteStockSQL = "DELETE FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
        rc = sqlite3_prepare_v2(db, deleteStockSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
            std::cerr << "Failed to prepare stock deletion: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, user_id);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE)
        {
            std::cerr << "Error deleting stock: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            return false;
        }
    }
    else
    {
        const char *updateStockSQL = "UPDATE Stocks SET stock_balance = stock_balance - ? WHERE stock_symbol = ? AND user_id = ?;";
        rc = sqlite3_prepare_v2(db, updateStockSQL, -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
        {
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

        if (rc != SQLITE_DONE)
        {
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
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare balance update: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_double(stmt, 1, total_earnings);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        std::cerr << "Error updating user balance: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    // Commit transaction so the sell actually works
    rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    sqlite3_close(db);
    return true;
}
bool depositAmount(int user_id, double amount, const std::string &dbName)
{
    sqlite3 *db;
    if (!openDatabase(&db, dbName)) return false;

    const char *sql = "UPDATE Users SET usd_balance = usd_balance + ? WHERE ID = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_double(stmt, 1, amount);
    sqlite3_bind_int(stmt, 2, user_id);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return success;
}
double getUserBalance(int user_id, const std::string &dbName)
{
    sqlite3 *db;
    if (!openDatabase(&db, dbName)) return -1;

    const char *query = "SELECT usd_balance FROM Users WHERE ID = ?;";
    sqlite3_stmt *stmt;
    double balance = -1;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            balance = sqlite3_column_double(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return balance;
}
std::string lookupStock(int user_id, const std::string &ticker, const std::string &dbName)
{
    sqlite3 *db;
    if (!openDatabase(&db, dbName)) return "";

    const char *sql = "SELECT stock_symbol, stock_balance FROM Stocks WHERE user_id = ? AND stock_symbol LIKE ?;";
    sqlite3_stmt *stmt;
    std::ostringstream stockInfo;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        std::string searchPattern = ticker + "%";
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            stockInfo << sqlite3_column_text(stmt, 0) << " " << sqlite3_column_double(stmt, 1) << "\n";
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return stockInfo.str();
}



bool listStock(const std::string &dbName, int user_id)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    
    if (!openDatabase(&db, dbName))
    {
        std::cerr << "Failed to open the database!" << std::endl;
        return false;
    }

    std::ostringstream query;
    
    if (user_id == 1) // Root user can list all stocks
    {
        query << "SELECT id, stock_symbol, stock_balance, user_id FROM Stocks;";
    }
    else // Regular users only see their own stocks
    {
        query << "SELECT id, stock_symbol, stock_balance, user_id FROM Stocks WHERE user_id = ?;";
    }

    const std::string queryStr = query.str();
    
    if (sqlite3_prepare_v2(db, queryStr.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Error preparing LIST query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    if (user_id != 1) // Only bind parameter for non-root users
    {
        sqlite3_bind_int(stmt, 1, user_id);
    }

    std::cout << "200 OK\nThe list of stocks:\n";

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const char *stock_symbol = (const char *)sqlite3_column_text(stmt, 1);
        double stock_balance = sqlite3_column_double(stmt, 2);
        int user_id = sqlite3_column_int(stmt, 3);

        if (user_id == 1) // Root user sees all records
        {
            std::cout << id << " " << stock_symbol << " " << stock_balance << " User: " << user_id << std::endl;
        }
        else // Regular user sees only their stocks
        {
            std::cout << id << " " << stock_symbol << " " << stock_balance << std::endl;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
}

bool getUserBalance(int user_id, std::string &first_name, std::string &last_name, double &usd_balance, const std::string &dbName)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    if (!openDatabase(&db, dbName))
        return false;

    const char *query = "SELECT first_name, last_name, usd_balance FROM Users WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        first_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        last_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        usd_balance = sqlite3_column_double(stmt, 2);
        found = true;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

bool checkCredentials(const std::string &userName, const std::string &password, int &user_id_out, const std::string &dbName)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    // Open the DB
    if (!openDatabase(&db, dbName)) {
        return false;
    }

    // Prepare SQL to find user by username + password
    const char *sql = "SELECT ID FROM Users WHERE user_name = ? AND password = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare login statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    // Bind the username and password
    sqlite3_bind_text(stmt, 1, userName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    bool valid = false;
    // If row exists -> credentials match
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id_out = sqlite3_column_int(stmt, 0);
        valid = true;
    }

    // Clean up
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return valid;
}


#endif