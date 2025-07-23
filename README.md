# 📡 Basic TCP Implementation (Reliable Data Transfer)

This project is a **basic implementation of TCP-like reliable data transfer**, inspired by **Section 3.7** of *Computer Networking: A Top-Down Approach* by James F. Kurose and Keith W. Ross (Addison-Wesley).

The goal is to demonstrate how reliable communication can be achieved over an unreliable network using core principles such as acknowledgments, retransmissions, and timeouts — all implemented in user space with C.

---

## 🧠 Protocol Logic

![Protocol Logic](Screenshot%202025-07-23%20231221.png)

---

## 🛠 How to Run

To simulate a fictional network with **5% packet loss** and run the sender/receiver applications, follow these steps on a **Linux** machine.

### 1. 🧱 Build the project

```bash
make
```

---

### 2. 🌐 Set up the virtual network

```bash
./network.sh up
```

---

### 3. 📥 Start the Receiver (on h2)

```bash
./network.sh 2       # Opens a shell on host h2

./receiver -v ra=10.0.2.1 rp=5000 sa=10.0.1.1 sp=5000 > output.txt
```

- `ra` and `rp`: receiver address and port  
- `sa` and `sp`: expected sender address and port  
- Output is saved to `output.txt`

---

### 4. 📤 Start the Sender (on h1)

```bash
./network.sh 1       # Opens a shell on host h1

./sender -v ra=10.0.2.1 rp=5000 sa=10.0.1.1 sp=5000 < 1mb.txt
```

- Sends `1mb.txt` from h1 to h2 using a custom reliable transfer protocol.

---

### 5. 🧹 Shut down the network

```bash
./network.sh down
```

---

## 🔍 Description

- This project mimics the Go-Back-N/Stop-and-Wait TCP behavior, managing:
  - **Sequence numbers**
  - **Acknowledgments (ACKs)**
  - **Packet retransmissions on timeout**
  - **End-to-end reliability over lossy links**
- The communication assumes a fixed IP and port for each end and manually streams files with loss detection and recovery.

---

## 📘 Reference

> Kurose, J.F., & Ross, K.W. (2021). *Computer Networking: A Top-Down Approach* (8th Edition). Addison-Wesley.

---

## 🧪 Notes

- Works in the provided virtual network setup (via `network.sh`) or can be run locally without any params
- Image shows the protocol logic used internally (e.g., sequence handling, ACK flow)
- This is a learning tool — not intended for production use

---

## 👤 Author

Developed as part of a study exercise to understand how reliable transport protocols work at a low level.
