
#include <ti/drivers/dpl/MutexP.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>


/*
 *  ======== MutexP_create ========
 */
MutexP_Handle MutexP_create(MutexP_Params *params)
{
    SemaphoreHandle_t sem = NULL;

    /*
     *  NOTE:  Documentation in semphr.h says that configUSE_RECURSIVE_MUTEXES
     *  must be set to 1 in FreeRTOSConfig.h  for this to be available, but
     *  the xSemaphore recursive calls are inside a configUSE_RECURSIVE_MUTEXES
     *  block.
     */
    sem = xSemaphoreCreateRecursiveMutex();

    return ((MutexP_Handle)sem);
}

/*
 *  ======== MutexP_delete ========
 */
void MutexP_delete(MutexP_Handle handle)
{
    vSemaphoreDelete((SemaphoreHandle_t)handle);
}

/*
 *  ======== MutexP_lock ========
 */
uintptr_t MutexP_lock(MutexP_Handle handle)
{
    SemaphoreHandle_t xMutex = (SemaphoreHandle_t)handle;

    /* Retry every 10 ticks */
    while (xSemaphoreTakeRecursive(xMutex, (TickType_t)10) == pdFALSE) {
        ;
    }

    return (0);
}

/*
 *  ======== MutexP_Params_init ========
 */
void MutexP_Params_init(MutexP_Params *params)
{
    params->callback = NULL;
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
/*
 *  ======== MutexP_staticObjectSize ========
 */
size_t MutexP_staticObjectSize(void)
{
    return (sizeof(StaticSemaphore_t));
}
#endif

/*
 *  ======== MutexP_unlock ========
 */
void MutexP_unlock(MutexP_Handle handle, uintptr_t key)
{
    SemaphoreHandle_t xMutex = (SemaphoreHandle_t)handle;
    xSemaphoreGiveRecursive(xMutex);
}
