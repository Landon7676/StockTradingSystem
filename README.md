## **Contributors**

- Landon Johnson (landonj@umich.edu)
- Jackson Harwick (jharwick@umich.edu)

# Stock Trading System

A simple stock trading system with a **client-server** architecture using **SQLite3**.

## **Setup Instructions**

### **1. Install Required Dependencies**

Before running the project, install **SQLite3** (needed for the database).

#### **For Ubuntu/WSL:**

```sh
sudo apt update && sudo apt install sqlite3 libsqlite3-dev -y
```

#### **For macOS (Homebrew):**

```sh
brew install sqlite3
```

#### **For Windows (MinGW or MSYS2):**

1. Download **SQLite3** from [sqlite.org](https://www.sqlite.org/download.html).
2. Install it and add it to your system's PATH.

---

### **2. Clone the Repository**

```sh
git clone https://github.com/Landon7676/StockTradingSystem.git
cd StockTradingSystem
```

---

### **3. Compile the Project Manually**

#### **Compile SQLite separately**

```sh
gcc -c sqlite3.c -o sqlite3.o -lpthread -ldl
```

#### **Compile the Server**

```sh
g++ -o server server.cpp database.cpp sqlite3.o -lpthread -ldl
```

#### **Compile the Client**

```sh
g++ -o client client.cpp
```

---

### **4. Run the Server**

```sh
./server
```

---

### **5. Run the Client**

```sh
./client localhost
```

Replace `localhost` with the **server IP** if running on a separate machine.

---

### **6. Clean Up**

To remove compiled files:

```sh
rm -f server client sqlite3.o
```

---

## **Student Roles**
