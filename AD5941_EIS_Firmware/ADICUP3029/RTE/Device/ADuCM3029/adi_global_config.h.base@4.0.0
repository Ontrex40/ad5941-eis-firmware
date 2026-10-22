/*! ****************************************************************************
 * @file:    adi_global_config.h
 * @brief:   Configuration options for all the drivers.
 * -----------------------------------------------------------------------------
 * Copyright (c) 2016-2020 Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * For license terms and conditions, please refer to the '*EZDFP Click Thru SLA'
 * file in the License subdirectory of this pack.
 *
 ******************************************************************************/
#ifndef ADI_GLOBAL_CONFIG_H
#define ADI_GLOBAL_CONFIG_H

/** @addtogroup GLOBAL_Driver_Config Global Static Configuration
 *  @brief   Configuration options for all the drivers.
 *  @{
 */

/*! Set this macro to 1 to enforce bus error on CRC error by default */
#define ADI_FLCC_ENABLE_BUS_ERR	(0u)

/*! Set this macro to 1 to enable SRAM parity by default */
#define ADI_SRAM_PARITY_ENABLE	(0u)

/*! @name RTOS used
 * In order to be used in a multi-threaded application, the device drivers
 * may require the use of some RTOS-specific signals like semaphores or actions
 * may be required when entering/exiting an interrupt. By specifying the RTOS
 * that the application uses, the drivers can map their requirements to the
 * specific RTOS, without requiring an OS abstraction layer.
 * @note This macros do not add the RTOS sources to the application, users need
 *       to set up the source and include paths in their application themselves
 * @note If the RTOS specified is not in the list of supported RTOS the build
 *       mechanism fails
 */
/**@{*/

/*! @hideinitializer Indicates that no RTOS is used (bare-metal applications) */
#define ADI_CFG_RTOS_NO_OS              (1)
/*! @hideinitializer Indicates that Micrium uCOS-III is used  */
#define ADI_CFG_RTOS_MICRIUM_III        (2)
/*! @hideinitializer Indicates that FreeRTOS is used */
#define ADI_CFG_RTOS_FREERTOS           (3)
/*! @hideinitializer Indicates that Express Logic ThreadX is used */
#define ADI_CFG_RTOS_THREADX            (4)
/*! @hideinitializer Indicates that Micrium uCOS-II is used  */
#define ADI_CFG_RTOS_MICRIUM_II         (5)

/*! Configure the RTOS required across the project.
    It can be configured to one of the following macros:
    - #ADI_CFG_RTOS_NO_OS
    - #ADI_CFG_RTOS_MICRIUM_III
    - #ADI_CFG_RTOS_FREERTOS
    - #ADI_CFG_RTOS_THREADX
    - #ADI_CFG_RTOS_MICRIUM_II
 */

#define ADI_CFG_RTOS    ADI_CFG_RTOS_NO_OS

/**@}*/

/*! @name Low power mode support
    All applications may have to block when a buffer is being processed. In the
    case of an RTOS application, when a task is blocked waiting for a buffer, a
    different task can run. If no tasks are available then the idle task runs.
    In many RTOS the idle task can be configured so it perform actions like
    entering low power modes.

    In the case of a bare-metal (no RTOS) application, since there are no other
    tasks to be run, the driver can enter low power modes itself when it blocks.
   */

/*! Configures the drivers to enter low power mode (Flexi mode)
    when waiting for a buffer to be processed. This macro is applicable
    only when the drivers are operating in the bare metal mode (No RTOS).

    The possible values it can be configured to are:

    - 1 : Low power mode support required.
    - 0 : Low power mode support not required.
*/

#if ((ADI_CFG_RTOS != ADI_CFG_RTOS_NO_OS))
#define ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT      (0)
#else
#define ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT      (1)
#endif
/**@}*/



/*
** Verify the macro configuration
*/

#if ((ADI_CFG_RTOS != ADI_CFG_RTOS_NO_OS)        && \
     (ADI_CFG_RTOS != ADI_CFG_RTOS_MICRIUM_II)   && \
     (ADI_CFG_RTOS != ADI_CFG_RTOS_MICRIUM_III)  && \
     (ADI_CFG_RTOS != ADI_CFG_RTOS_THREADX)      && \
     (ADI_CFG_RTOS != ADI_CFG_RTOS_FREERTOS))
#error "ADI_CFG_RTOS macro wrongly configured"
#endif /* ADI_CFG_RTOS verification */

#if ((ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT != 0) && \
     (ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT != 1))
#error "ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT macro is wrongly configured"
#endif

#if ((ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT == 1) && (ADI_CFG_RTOS != ADI_CFG_RTOS_NO_OS))
#error "ADI_CFG_ENTER_LOW_PWR_MODE_SUPPORT cannot be set to 1 in multi-threaded applications"
#endif
/**
 *  @}
 */

#endif /* ADI_GLOBAL_CONFIG_H */
