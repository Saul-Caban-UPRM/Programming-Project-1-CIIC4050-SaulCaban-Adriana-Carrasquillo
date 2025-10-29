#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>

#define PLANES_LIMIT 20
#define SH_MEMORY_NAME "/SharedMemory"
int planes = 0;
int takeoffs = 0;
int traffic = 0;
int *arr;
void Traffic(int signum) {
  int waiting = planes - takeoffs;
  if (waiting < 0) {
    waiting = 0;
  }
  if (waiting >= 10) {
    /* Message format expected by tests */
    printf("RUNWAY OVERLOADED!!!! \n");
    fflush(stdout);
  }
  if (planes < PLANES_LIMIT) {
    int can_add = PLANES_LIMIT - planes;
    int add;
    if (can_add >= 5) {
      add = 5;
    } else {
      add = can_add;
    }

    if (add > 0) {
      planes += add;
      pid_t radio_pid = (pid_t)arr[1];
      /* arr contains only PIDs; radio expected in slot 1 */
      kill(radio_pid, SIGUSR2);
    }
  }
}
void HandleSignal(int signal) {
  if (signal == SIGTERM) {
    printf("finalization of operations...\n");
    munmap(arr, 3 * sizeof(int));
    shm_unlink(SH_MEMORY_NAME);
    exit(0);
  }
  if (signal == SIGUSR1) {
    takeoffs += 5;
  }
}


int main(int argc, char *argv[]) {
  int fd = shm_open(SH_MEMORY_NAME, O_RDWR, 0666);
  if (fd == -1) {
    perror("shm_open failed");
    exit(EXIT_FAILURE);
  }

  arr = mmap(NULL, 3 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (arr == MAP_FAILED) {
    perror("mmap failed");
    close(fd);
    exit(EXIT_FAILURE);
  }

  /* we don't need the fd after mmap */
  close(fd);

  /* store this process PID into slot 2 */
  arr[2] = (int)getpid();

  struct sigaction sa;  // Name of variable
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = HandleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {  // put the right signal!
    perror("sigaction SIGUSR1 failed");
    exit(1);
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {  // put the right signal!
    perror("sigaction SIGTERM failed");
    exit(1);
  }

  /* configure timer to call Traffic via SIGALRM */
  struct sigaction sa_timer;
  memset(&sa_timer, 0, sizeof(sa_timer));
  sa_timer.sa_handler = Traffic;
  sigemptyset(&sa_timer.sa_mask);
  sa_timer.sa_flags = 0;
  if (sigaction(SIGALRM, &sa_timer, NULL) == -1) {
    perror("sigaction SIGALRM failed");
    exit(1);
  }

  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 500000;  // 500 ms
  timer.it_interval = timer.it_value;
  setitimer(ITIMER_REAL, &timer, NULL);

  /* keep running so timer and handlers execute */
  while (1) {
    pause();
  }

  return 0;
}