# Inter-Process Communication
  
This Project simulates a restaurant (BarFooBar) using **System V shared memory** and **semaphores** for inter-process communication and synchronization.

## Overview
- **Entities**: 2 cooks (C, D), 5 waiters (U–Y), multiple customers.
- **Constraints**: 10 tables, each for 1–4 persons.
- **Workflow**:
  - Customers arrive (from `customers.txt`).
  - Waiters take orders in round-robin fashion.
  - Cooks prepare food (5 minutes/person).
  - Customers eat (30 minutes fixed).
- **Time Simulation**: 1 simulated minute = 100ms real time.

## Programs
- `cook.c` → Wrapper that creates 2 cook processes.
- `waiter.c` → Wrapper that creates 5 waiter processes.
- `customer.c` → Reads input file and spawns customer processes.
- `gencustomers.c` → Random customer generator (produces `customers.txt`).
