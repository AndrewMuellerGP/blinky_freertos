
#include "FreeRTOS.h"
#include "task.h"

#include <ti/drivers/Power.h>

//#include <ti/devices/msp432p4xx/driverlib/cpu.h>


void PowerMSP430_deepSleepPolicy()
{
    //uint32_t constraints;
    //bool slept = false;

    // disable interrupts
    //CPU_cpsid();
    //_disable_interrupts();

    // disable Task scheduling
    vTaskSuspendAll();

    /* query the declared constraints */
    //constraints = Power_getConstraintMask();

    /*
     *  Check if can go to a sleep state, starting with the deepest level.
     *  Do not go to a sleep state if a lesser sleep state is disallowed.
     */

     /* check if can go to DEEPSLEEP_1 */
    //if ((constraints & ((1 << PowerMSP432_DISALLOW_SLEEP) |
    //                    (1 << PowerMSP432_DISALLOW_DEEPSLEEP_0) |
    //                    (1 << PowerMSP432_DISALLOW_DEEPSLEEP_1))) == 0)
    {

        /* go to DEEPSLEEP_1 */
        //Power_sleep(PowerMSP432_DEEPSLEEP_1);
       //LPM3;

        /* set 'slept' to true*/
        //slept = true;
    }

    /* if didn't sleep yet, now check if can go to DEEPSLEEP_0 */
    //if (!slept && ((constraints & ((1 << PowerMSP432_DISALLOW_SLEEP) |
    //                    (1 << PowerMSP432_DISALLOW_DEEPSLEEP_0))) == 0))
    {

        /* go to DEEPSLEEP_0 */
        //Power_sleep(PowerMSP432_DEEPSLEEP_0);
       //LPM3;

        /* set 'slept' to true*/
        //slept = true;
    }

    /* if didn't sleep yet, now check if can go to SLEEP */
    //if (!slept && ((constraints & (1 << PowerMSP432_DISALLOW_SLEEP)) == 0))
    {

        /* go to SLEEP */
        //Power_sleep(PowerMSP432_SLEEP);
        //LPM3;

        /* set 'slept' to true*/
        //slept = true;
    }

    /* re-enable interrupts */
    //CPU_cpsie();
    //_enable_interrupts();

    /* restore Task scheduling */
    xTaskResumeAll();

    // if didn't sleep yet, just do WFI
    /*if (!slept) {
        __asm(" wfi");
    }*/
}

/*
 *  ======== PowerMSP432_sleepPolicy ========
 */
void PowerMSP430_sleepPolicy()
{
    //uint32_t constraints;
    //bool slept = false;

    /* disable interrupts */
    //CPU_cpsid();
    //_disable_interrupts();

    /* disable Task scheduling */
    vTaskSuspendAll();

    /* query the declared constraints */
    //constraints = Power_getConstraintMask();

    // sleep, if there is no constraint prohibiting it
    //if ((constraints & (1 << PowerMSP432_DISALLOW_SLEEP)) == 0)
    {

        // go to SLEEP
        //Power_sleep(PowerMSP432_SLEEP);
        //LPM3;

        // set 'slept' to true
        //slept = true;
    }

    // re-enable interrupts
    //CPU_cpsie();
    //_enable_interrupts();


    // restore Task scheduling
    xTaskResumeAll();

    // if didn't sleep yet, just do WFI
    /*if (!slept) {
        __asm(" wfi");
    }*/
}

void PowerMSP430_initPolicy()
{
}

void PowerMSP430_schedulerDisable()
{
    vTaskSuspendAll();
}

/*
 *  ======== PowerMSP432_schedulerRestore ========
 */
void PowerMSP430_schedulerRestore()
{
    xTaskResumeAll();
}

/*
 *  ======== PowerMSP432_updateFreqs ========
 */
/*void PowerMSP430_updateFreqs(PowerMSP432_Freqs *freqs)
{
}*/

/* Tickless Hook */
/*void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
#if (configUSE_TICKLESS_IDLE != 0)
    Power_idleFunc();
#endif
}*/
