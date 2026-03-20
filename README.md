# Skibus Simulation

---

## 📌 Project Description

This project simulates the operation of a skibus transporting skiers between multiple bus stops and a final destination.

The implementation uses:
- **Processes (`fork`)**
- **Shared memory (`mmap`, `shm_open`)**
- **Semaphores (`sem_t`, `sem_open`)**

The main goal is to correctly synchronize multiple skier processes and one skibus process while avoiding race conditions and inconsistent shared data access.

---

## ⚙️ Program Behavior

### 👤 Skiers
- Each skier is represented by a separate process
- After a random delay (0–TL), the skier arrives at a random bus stop
- The skier waits for the skibus
- After boarding, the skier waits until reaching the final station

---

### 🚌 Skibus
- Travels sequentially through stops: `1 → Z`
- At each stop:
  - Picks up waiting skiers (up to capacity `K`)
- At the final stop:
  - All skiers leave the bus ("going to ski")
- The process repeats until all skiers are transported

---
```
./proj2 L Z K TL TB

```
| Parameter | Description |
|----------|------------|
| `L` | Number of skiers (`L < 20000`) |
| `Z` | Number of bus stops (`1 ≤ Z ≤ 10`) |
| `K` | Bus capacity (`10 ≤ K ≤ 100`) |
| `TL` | Max delay before skier arrives (microseconds) |
| `TB` | Max travel time between stops (microseconds) |

