#ifndef AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_
#define AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_

void *TakeOffsFunction(void *);
void MemoryCreate();
void SigHandler2(int signal);

#define SH_MEMORY_NAME "/SharedMemory"
#define TOTAL_TAKEOFFS 20

extern int *arr;

#endif  // AIR_CONTROL_C_INCLUDE_FUNCTIONS_H_