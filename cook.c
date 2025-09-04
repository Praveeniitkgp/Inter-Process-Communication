#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>

#define SHM_SIZE 2000 * sizeof(int) // Shared memory size
#define MINUTE 100000 // 100 ms in microseconds

// Semaphore operations
struct sembuf sem_wait = {0, -1, SEM_UNDO};
struct sembuf sem_signal = {0, 1, SEM_UNDO};

// Shared memory structure
int *shm;
int shmid, semid_mutex, semid_cook;

// Time conversion function
void print_time(int minutes) {
    int hour = 11 + minutes / 60;
    int min = minutes % 60;
    printf("[%02d:%02d %s] ", hour, min, (hour < 12 || (hour == 12 && min == 0)) ? "am" : "pm");
}

// Cook main function
void cmain(int cook_id) {
    char cook_name = (cook_id == 0) ? 'C' : 'D';
    print_time(0);
    printf("Cook %c is ready\n", cook_name);
    fflush(stdout);

    while (1) {
        semop(semid_cook, &sem_wait, 1); // Wait for cooking request
        semop(semid_mutex, &sem_wait, 1); // Lock shared memory

        if (shm[0] >= 240 && shm[3] == 0) { // After 3:00 pm and no orders
            if (cook_id == 1) { // Last cook wakes waiters
                for (int i = 0; i < 5; i++) {
                    semctl(semid_cook + 1 + i, 0, SETVAL, 1);
                }
            }
            semop(semid_mutex, &sem_signal, 1);
            print_time(shm[0]);
            printf("Cook %c: Leaving\n", cook_name);
            fflush(stdout);
            exit(0);
        }

        int front = shm[1100]; // Cook queue front
        int waiter_id = shm[1102 + 3 * front];
        int customer_id = shm[1102 + 3 * front + 1];
        int count = shm[1102 + 3 * front + 2];
        shm[1100]++; // Move front
        shm[3]--; // Decrease pending orders

        print_time(shm[0]);
        printf("Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n",
               cook_name, 'U' + waiter_id, customer_id, count);
        fflush(stdout);

        semop(semid_mutex, &sem_signal, 1); // Unlock shared memory

        int curr_time = shm[0];
        usleep(5 * count * MINUTE); // 5 minutes per person
        semop(semid_mutex, &sem_wait, 1);
        shm[0] = curr_time + 5 * count;
        semop(semid_mutex, &sem_signal, 1);

        semop(semid_mutex, &sem_wait, 1);
        print_time(shm[0]);
        printf("Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n",
               cook_name, 'U' + waiter_id, customer_id, count);
        fflush(stdout);

        shm[100 + 200 * waiter_id + 198] = customer_id; // Set FR
        semop(semid_cook + 1 + waiter_id, &sem_signal, 1); // Signal waiter
        semop(semid_mutex, &sem_signal, 1);
    }
}

int main() {
    // Create shared memory
    key_t key = ftok(".", 'R');
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    shm = (int *)shmat(shmid, NULL, 0);

    // Initialize shared memory
    shm[0] = 0; // time
    shm[1] = 10; // empty tables
    shm[2] = 0; // next waiter
    shm[3] = 0; // pending orders
    shm[1100] = 0; // cook queue front
    shm[1101] = 0; // cook queue back
    for (int i = 0; i < 5; i++) {
        shm[100 + 200 * i] = 0; // waiter queue front
        shm[101 + 200 * i] = 0; // waiter queue back
        shm[102 + 200 * i] = 0; // PO
    }

    // Create semaphores
    semid_mutex = semget(key, 1, IPC_CREAT | 0666);
    semctl(semid_mutex, 0, SETVAL, 1);
    semid_cook = semget(key + 1, 6, IPC_CREAT | 0666); // 1 for cook, 5 for waiters
    semctl(semid_cook, 0, SETVAL, 0);
    for (int i = 1; i <= 5; i++) {
        semctl(semid_cook, i, SETVAL, 0);
    }

    // Fork cook processes
    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            cmain(i);
        }
    }

    // Wait for cooks to terminate
    wait(NULL);
    wait(NULL);
    return 0;
}