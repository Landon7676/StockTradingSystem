## **Contributors**

- Landon Johnson (landonj@umich.edu)
- Jackson Harwick (@jharwick23)

# Stock Trading System

A simple stock trading system with a **client-server** architecture.

## **Introduction**

This project is written in **C++** and is designed to work on **Linux, macOS, and Windows**. It implements a stock trading system using a client-server model with local SQLite database integration.

## **Setup Instructions**

### **1. Clone the Repository**

```sh
git clone https://github.com/Landon7676/StockTradingSystem.git
cd StockTradingSystem
```

---

### **2. Compile the Project Manually**

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

To remove compiled files:

```sh
rm -f server client sqlite3.o
```

---

## **Student Roles**

### Jackson Harwick

- Worked on **BUY**, **SELL**, **SHUTDOWN**, and **QUIT** commands.
- Wrote the **README** file.
- Implemented the above commands into both the **client** and **server**.

### Landon Johnson
- Implemented the initial **Client.cpp** and **Server.cpp** files
- Worked on **LIST** and **BALANCE** commands
- Implemented these commands into the **client** and **server**

## **Bugs**

-Header file acting like it's not included but still working when included.