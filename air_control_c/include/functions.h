#ifndef INCLUDE_FUNCTIONS_H_
#define INCLUDE_FUNCTIONS_H_
void *TakeOffsFunction(void *);
void MemoryCreate();
void SigHandler2(int signal);
#define SH_MEMORY_NAME "/SharedMemory"
#define TOTAL_TAKEOFFS 5
extern int *arr;
#endif // INCLUDE_FUNCTIONS_H_