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

### **2. Install Make**

If `make` is not installed, install it using one of the following methods:

#### **For Ubuntu/WSL:**

```sh
sudo apt update && sudo apt install make -y
```

#### **For macOS (Homebrew):**

```sh
brew install make
```

#### **For Windows (Chocolatey):**

```sh
choco install make -y
```

#### **For Windows (MinGW or MSYS2):**

```sh
pacman -S make
```

---

### **3. Clone the Repository**

```sh
git clone https://github.com/Landon7676/StockTradingSystem.git
cd StockTradingSystem
```

---

### **4. Compile the Project**

```sh
make
```

---

### **5. Run the Server**

```sh
./server
```

---

### **6. Run the Client**

```sh
./client localhost
```

Replace `localhost` with the **server IP** if running on a separate machine.

---

### **7. Clean Up**

To remove compiled files:

```sh
make clean
```

## **Contributors**

- Landon7676 (@Landon7676)
- jharwick23 (@jharwick23)

---
