#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>

bool initializeDatabase(const std::string &dbName);
bool addUser(const std::string &first_name, const std::string &last_name,
             const std::string &user_name, const std::string &password, double balance);
void listUsers();
double getUserBalance(int userId);
bool updateUserBalance(int userId, double newBalance);

#endif