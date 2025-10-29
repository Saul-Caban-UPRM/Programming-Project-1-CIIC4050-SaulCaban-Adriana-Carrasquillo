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
#include "functions.h"

int planes = 0;
int takeoffs = 0;
int total_takeoffs = 0;
int *arr = NULL;

/* runway locks and a main state lock */
static pthread_mutex_t runway1_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t runway2_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

void MemoryCreate() {
  int fd = shm_open(SH_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
  ftruncate(fd, 3 * sizeof(int));
  arr = mmap(0, 3 * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (arr == MAP_FAILED) {
    perror("mmap failed");
    exit(EXIT_FAILURE);
  }
  arr[0] = getpid();
}

void SigHandler2(int signal) {
  /* add 5 planes as required by SIGUSR2 */
  pthread_mutex_lock(&state_lock);
  planes += 5;
  pthread_mutex_unlock(&state_lock);
}

void *TakeOffsFunction(void *arg) {
  int id = *((int *)arg);
  free(arg);

  while (1) {
    /* quick termination check */
    pthread_mutex_lock(&state_lock);
    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      break;
    }
    pthread_mutex_unlock(&state_lock);

    int got_runway = 0;

    /* try runway 1 then runway 2 (simple strategy) */
    if (pthread_mutex_trylock(&runway1_lock) == 0) {
      got_runway = 1;
    } else if (pthread_mutex_trylock(&runway2_lock) == 0) {
      got_runway = 2;
    } else {
      /* both busy, wait and retry */
      sleep(1);
      continue;
    }

    /* now we hold a runway lock; enter protected state to update counters */
    pthread_mutex_lock(&state_lock);

    /* double-check termination after acquiring locks */
    if (total_takeoffs >= TOTAL_TAKEOFFS) {
      pthread_mutex_unlock(&state_lock);
      if (got_runway == 1) {
        pthread_mutex_unlock(&runway1_lock);
      } else {
        pthread_mutex_unlock(&runway2_lock);
      }
      break;
    }

    if (planes <= 0) {
      /* no plane available right now: release locks and retry later */
      pthread_mutex_unlock(&state_lock);
      if (got_runway == 1) {
        pthread_mutex_unlock(&runway1_lock);
      } else {
        pthread_mutex_unlock(&runway2_lock);
      }

      sleep(1);
      continue;
    }

    /* perform takeoff: update shared counters */
    planes -= 1;
    takeoffs += 1;
    total_takeoffs += 1;

    /* if 5 takeoffs completed, notify radio and reset takeoffs */
    if (takeoffs >= 5) {
      if (arr != NULL && arr[1] > 0) {
        kill((pid_t)arr[1], SIGUSR1);
      }
      takeoffs = 0;
    }

    /* release main state lock as required */
    pthread_mutex_unlock(&state_lock);

    /* simulate takeoff time */
    sleep(1);

    /* release the runway lock we held */
    if (got_runway == 1) {
      pthread_mutex_unlock(&runway1_lock);
    } else {
      pthread_mutex_unlock(&runway2_lock);
    }
  }

  /* finished total_takeoffs: notify radio to 
  terminate, unmap shared mem and return */
  if (arr != NULL && arr[1] > 0) {
    kill((pid_t)arr[1], SIGTERM);
  }

  if (arr != NULL) {
    munmap(arr, 3 * sizeof(int));
    arr = NULL;
  }

  return NULL;
}