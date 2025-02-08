# **Stock Trading System**

## **Contributors**

- **Landon Johnson** (landonj@umich.edu)
- **Jackson Harwick** (jharwick@umich.edu)

---

## **Introduction**

This project is written in **C++** and is designed to work on **Linux, macOS, and Windows**. It implements a stock trading system using a **client-server** model with local **SQLite** database integration.

---

## **Setup Instructions**

### **1. Clone the Repository**

```sh
git clone https://github.com/Landon7676/StockTradingSystem.git
cd StockTradingSystem
```

---

### **2. Compile the Project**

You can compile the project using **Makefile** or manually.

#### **Using Makefile**

To compile both the **server** and **client**, simply run:

```sh
make
```

This will compile the SQLite object file and both the server and client programs.

#### **Manual Compilation**

If you prefer manual compilation, follow these steps:

##### **Compile SQLite separately**

```sh
gcc -c sqlite3.c -o sqlite3.o -lpthread -ldl
```

##### **Compile the Server**

```sh
g++ -o server server.cpp database.cpp sqlite3.o -lpthread -ldl
```

##### **Compile the Client**

```sh
g++ -o client client.cpp
```

---

### **3. Run the Server**

```sh
./server
```

---

### **4. Run the Client**

```sh
./client localhost
```

Replace `localhost` with the **server IP** if running on a separate machine.

---

### **5. Clean Up**

To remove compiled files, use:

```sh
rm -f server client sqlite3.o
```

---

## **Student Roles**

### **Jackson Harwick**

- Implemented **BUY**, **SELL**, **SHUTDOWN**, and **QUIT** commands.
- Wrote the **README** file.
- Integrated these commands into both the **client** and **server**.

### **Landon Johnson**

- Developed initial **client.cpp** and **server.cpp** files.
- Implemented **LIST** and **BALANCE** commands.
- Integrated these commands into the **client** and **server**.

---

## **Bugs**

- Header file behaves as if it is not included but still functions when explicitly included.
- Needed to specify version of C++ to compile server

---

## **Demo**

- https://youtu.be/x7-BhXH6xHs

---
