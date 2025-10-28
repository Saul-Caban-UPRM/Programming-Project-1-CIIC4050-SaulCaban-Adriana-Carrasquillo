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

int main()
{
  // TODO 1: Call the function that creates the shared memory segment.

  // TODO 3: Configure the SIGUSR2 signal to increment the planes on the runway
  // by 5.

  // TODO 4: Launch the 'radio' executable and, once launched, store its PID in
  // the second position of the shared memory block.

  // TODO 6: Launch 5 threads which will be the controllers; each thread will
  // execute the TakeOffsFunction().
  MemoryCreate();

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SigHandler2;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGUSR2, &sa, NULL) == -1)
  { // put the right signal!
    perror("sigaction failed\n");
    exit(1);
  }
  pid_t child_A = fork();
  if (child_A == -1)
  {
    perror("fork failed");
    exit(1);
  }

  if (child_A == 0)
  {
    /* child: exec radio (pid remains the same after exec) */
    execlp("./radio", "radio", SH_MEMORY_NAME, NULL);
    perror("execlp child_A failed");
    _exit(1);
  }

  /* parent: store the radio PID (child PID) in shared memory */
  arr[1] = (int)child_A;

  pthread_t controller[5];

  for (int i = 0; i < 5; i++)
  {
    int *id = malloc(sizeof(int));
    *id = i;
    pthread_create(&controller[i], NULL, TakeOffsFunction, id);
  }

  for (int i = 0; i < 5; i++)
  {
    pthread_join(controller[i], NULL);
  }
  shm_unlink(SH_MEMORY_NAME);
  return 0;
}