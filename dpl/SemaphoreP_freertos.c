#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/drivers/dpl/HwiP.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>

/*
 *  Maximum count for a semaphore.
 */
#define MAXCOUNT 0xffff


/*
 *  ======== SemaphoreP_create ========
 */
SemaphoreP_Handle SemaphoreP_create(unsigned int count,
                                    SemaphoreP_Params *params)
{
    SemaphoreHandle_t sem = NULL;
    SemaphoreP_Params semParams;

    if (params == NULL) {
        params = &semParams;
        SemaphoreP_Params_init(params);
    }

    if (params->mode == SemaphoreP_Mode_COUNTING) {
#if (configUSE_COUNTING_SEMAPHORES == 1)
        /*
         *  The size of the semaphore queue is not dependent on MAXCOUNT.
         *
         *  FreeRTOS xSemaphoreCreateCounting() appears from the
         *  code in xQueueCreateCountingSemaphore() to create
         *  a queue of length maxCount, where maxCount is the
         *  maximum count that the semaphore should ever reach.
         *  However, the queue item size (queueSEMAPHORE_QUEUE_ITEM_LENGTH),
         *  is 0, so no actual memoory is allocated for the queue items.
         *  Therefore we can pass any non-zero number as the maximum
         *  semaphore count.
         */
        sem = xSemaphoreCreateCounting((UBaseType_t)MAXCOUNT,
                (UBaseType_t)count);
#endif
    }
    else {
        sem = xSemaphoreCreateBinary();
        if ((sem != NULL) && (count != 0)) {
            xSemaphoreGive(sem);
        }
    }

    return ((SemaphoreP_Handle)sem);
}

/*
 *  ======== SemaphoreP_createBinary ========
 */
SemaphoreP_Handle SemaphoreP_createBinary(unsigned int count)
{
    SemaphoreHandle_t sem = NULL;

    sem = xSemaphoreCreateBinary();
    if ((sem != NULL) && (count != 0)) {
        xSemaphoreGive(sem);
    }

    return ((SemaphoreP_Handle)sem);
}

/*
 *  ======== SemaphoreP_delete ========
 */
void SemaphoreP_delete(SemaphoreP_Handle handle)
{
    vSemaphoreDelete((SemaphoreHandle_t)handle);
}

/*
 *  ======== SemaphoreP_Params_init ========
 */
void SemaphoreP_Params_init(SemaphoreP_Params *params)
{
    params->mode = SemaphoreP_Mode_COUNTING;
    params->callback = NULL;
}

/*
 *  ======== SemaphoreP_pend ========
 */
SemaphoreP_Status SemaphoreP_pend(SemaphoreP_Handle handle, uint32_t timeout)
{
    BaseType_t status;

    if (HwiP_inISR()) {
        /* In ISR */
        status = xSemaphoreTakeFromISR((SemaphoreHandle_t)handle, NULL);
    }
    else {
        status = xSemaphoreTake((SemaphoreHandle_t)handle, timeout);
    }

    if (status == pdTRUE) {
        return (SemaphoreP_OK);
    }

    return (SemaphoreP_TIMEOUT);
}

/*
 *  ======== SemaphoreP_post ========
 */
void SemaphoreP_post(SemaphoreP_Handle handle)
{
    BaseType_t xHigherPriorityTaskWoken;

    if (!HwiP_inISR()) {
        /* Not in ISR */
        xSemaphoreGive((SemaphoreHandle_t)handle);
    }
    else {
        xSemaphoreGiveFromISR((SemaphoreHandle_t)handle,
                &xHigherPriorityTaskWoken);
    }
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)
/*
 *  ======== SemaphoreP_staticObjectSize ========
 */
size_t SemaphoreP_staticObjectSize(void)
{
    return (sizeof(StaticSemaphore_t));
}
#endif
