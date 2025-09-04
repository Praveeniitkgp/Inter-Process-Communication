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

void wmain(int waiter_id) {
    char waiter_name = 'U' + waiter_id;
    print_time(0);
    printf("Waiter %c is ready\n", waiter_name);
    fflush(stdout);

    while (1) {
        semop(semid_cook + 1 + waiter_id, &sem_wait, 1); // Wait for signal
        semop(semid_mutex, &sem_wait, 1);

        if (shm[0] >= 240 && shm[102 + 200 * waiter_id] == 0) { // After 3:00 pm and no pending
            semop(semid_mutex, &sem_signal, 1);
            print_time(shm[0]);
            printf("\t\tWaiter %c leaving (no more customer to serve)\n", waiter_name);
            fflush(stdout);
            exit(0);
        }

        int fr = shm[100 + 200 * waiter_id + 198]; // Food ready customer ID
        if (fr > 0) {
            print_time(shm[0]);
            printf("\tWaiter %c: Serving food to Customer %d\n", waiter_name, fr);
            fflush(stdout);
            shm[100 + 200 * waiter_id + 198] = 0; // Clear FR
            semop(semid_cook + 6 + fr, &sem_signal, 1); // Signal customer
        } else if (shm[102 + 200 * waiter_id] > 0) {
            int front = shm[100 + 200 * waiter_id];
            int customer_id = shm[104 + 200 * waiter_id + 2 * front];
            int count = shm[105 + 200 * waiter_id + 2 * front];
            shm[100 + 200 * waiter_id]++; // Move front
            shm[102 + 200 * waiter_id]--; // Decrease PO

            print_time(shm[0]);
            printf("\tWaiter %c: Placing order for Customer %d (count = %d)\n",
                   waiter_name, customer_id, count);
            fflush(stdout);

            int curr_time = shm[0];
            semop(semid_mutex, &sem_signal, 1);
            usleep(MINUTE); // 1 minute to take order
            semop(semid_mutex, &sem_wait, 1);
            shm[0] = curr_time + 1;

            int back = shm[1101]; // Cook queue back
            shm[1102 + 3 * back] = waiter_id;
            shm[1102 + 3 * back + 1] = customer_id;
            shm[1102 + 3 * back + 2] = count;
            shm[1101]++; // Move back
            shm[3]++; // Increase pending orders
            semop(semid_cook, &sem_signal, 1); // Signal cook
        }
        semop(semid_mutex, &sem_signal, 1);
    }
}

int main() {
    key_t key = ftok(".", 'R');
    shmid = shmget(key, SHM_SIZE, 0666);
    shm = (int *)shmat(shmid, NULL, 0);
    semid_mutex = semget(key, 1, 0666);
    semid_cook = semget(key + 1, 6, 0666);

    for (int i = 0; i < 5; i++) {
        if (fork() == 0) {
            wmain(i);
        }
    }

    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }
    return 0;
}