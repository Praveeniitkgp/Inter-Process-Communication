#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define SHM_SIZE 2000 * sizeof(int)
#define MINUTE 100000

struct sembuf sem_wait = {0, -1, SEM_UNDO};
struct sembuf sem_signal = {0, 1, SEM_UNDO};

int *shm;
int shmid, semid_mutex, semid_cook;

void print_time(int minutes) {
    int hour = 11 + minutes / 60;
    int min = minutes % 60;
    printf("[%02d:%02d %s] ", hour, min, (hour < 12 || (hour == 12 && min == 0)) ? "am" : "pm");
}

void cmain(int customer_id, int arrival_time, int count) {
    semop(semid_mutex, &sem_wait, 1);
    if (arrival_time > 240) { // After 3:00 pm
        print_time(arrival_time);
        printf("\t\t\t\t\tCustomer %d leaves (late arrival)\n", customer_id);
        fflush(stdout);
        semop(semid_mutex, &sem_signal, 1);
        exit(0);
    }
    if (shm[1] == 0) { // No empty tables
        print_time(arrival_time);
        printf("\t\t\t\t\tCustomer %d leaves (no empty table)\n", customer_id);
        fflush(stdout);
        semop(semid_mutex, &sem_signal, 1);
        exit(0);
    }

    shm[0] = arrival_time;
    shm[1]--; // Occupy a table
    int waiter_id = shm[2];
    shm[2] = (shm[2] + 1) % 5;
    print_time(shm[0]);
    printf("Customer %d arrives (count = %d)\n", customer_id, count);
    fflush(stdout);

    int back = shm[101 + 200 * waiter_id];
    shm[104 + 200 * waiter_id + 2 * back] = customer_id;
    shm[105 + 200 * waiter_id + 2 * back] = count;
    shm[101 + 200 * waiter_id]++; // Move back
    shm[102 + 200 * waiter_id]++; // Increase PO
    semop(semid_cook + 1 + waiter_id, &sem_signal, 1); // Signal waiter
    semop(semid_mutex, &sem_signal, 1);

    semop(semid_cook + 6 + customer_id, &sem_wait, 1); // Wait for order taken
    semop(semid_mutex, &sem_wait, 1);
    print_time(shm[0]);
    printf("\tCustomer %d: Order placed to Waiter %c\n", customer_id, 'U' + waiter_id);
    fflush(stdout);
    semop(semid_mutex, &sem_signal, 1);

    semop(semid_cook + 6 + customer_id, &sem_wait, 1); // Wait for food
    semop(semid_mutex, &sem_wait, 1);
    int wait_time = shm[0] - arrival_time;
    print_time(shm[0]);
    printf("\t\tCustomer %d gets food [Waiting time = %d]\n", customer_id, wait_time);
    fflush(stdout);

    int curr_time = shm[0];
    semop(semid_mutex, &sem_signal, 1);
    usleep(30 * MINUTE); // 30 minutes to eat
    semop(semid_mutex, &sem_wait, 1);
    shm[0] = curr_time + 30;
    shm[1]++; // Free table
    print_time(shm[0]);
    printf("\t\t\tCustomer %d finishes eating and leaves\n", customer_id);
    fflush(stdout);
    semop(semid_mutex, &sem_signal, 1);
    exit(0);
}

int main() {
    key_t key = ftok(".", 'R');
    shmid = shmget(key, SHM_SIZE, 0666);
    shm = (int *)shmat(shmid, NULL, 0);
    semid_mutex = semget(key, 1, 0666);
    semid_cook = semget(key + 1, 6, 0666);
    int semid_customer = semget(key + 2, 200, IPC_CREAT | 0666);
    for (int i = 0; i < 200; i++) {
        semctl(semid_customer, i, SETVAL, 0);
    }
    semid_cook += (semid_customer - (key + 2)); // Adjust semid_cook for customer semaphores

    FILE *fp = fopen("customers.txt", "r");
    int customer_id, arrival_time, count, prev_time = 0;
    while (fscanf(fp, "%d", &customer_id) == 1 && customer_id != -1) {
        fscanf(fp, "%d %d", &arrival_time, &count);
        usleep((arrival_time - prev_time) * MINUTE); // Wait for arrival
        prev_time = arrival_time;
        if (fork() == 0) {
            cmain(customer_id, arrival_time, count);
        }
    }
    fclose(fp);

    while (wait(NULL) > 0); // Wait for all customers
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid_mutex, 0, IPC_RMID);
    semctl(semid_cook - (semid_customer - (key + 2)), 0, IPC_RMID);
    semctl(semid_customer, 0, IPC_RMID);
    return 0;
}