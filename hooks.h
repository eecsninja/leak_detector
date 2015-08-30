#ifndef _HOOKS_H_
#define _HOOKS_H_

#include <stddef.h>

namespace MallocHook {

typedef void (*NewHookType)(const void*, size_t);
typedef void (*DeleteHookType)(const void*);

NewHookType SetNewHook(NewHookType);
DeleteHookType SetDeleteHook(DeleteHookType);

void InvokeNewHook(const void* ptr, size_t size);
void InvokeDeleteHook(const void* ptr);

void SetCallerStackTrace(int depth, void* const stack[]);
int GetCallerStackTrace(void* stack[], int depth, int skip);

}  // namespace MallocHook

#endif  // _HOOKS_H_
