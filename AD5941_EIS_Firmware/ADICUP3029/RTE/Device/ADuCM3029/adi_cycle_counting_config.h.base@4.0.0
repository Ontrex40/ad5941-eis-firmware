/*! ****************************************************************************
 * @file    adi_cycle_counting_config.h
 * @brief   Cycle Counting Framework configuration
 * -----------------------------------------------------------------------------
 * Copyright (c) 2016-2019 Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * For license terms and conditions, please refer to the '*EZDFP Click Thru SLA'
 * file in the License subdirectory of this pack.
 *
 ******************************************************************************/

#ifndef ADI_CYCLE_COUNTING_CONFIG_H
#define ADI_CYCLE_COUNTING_CONFIG_H

/** @addtogroup CYCLE_COUNTING_Config Static Configuration
 *  @ingroup cyclecount_logging
 *  @{
 */


/************* Cycle Counting Configuration ***************/

/*! Global enable. This must be enabled for any other functionality to work\n
    0u disabled
	1u enabled
*/
#define ADI_CYCLECOUNT_ENABLED                          (0u)

/*! SPI Interrupt Mode ISR Cycle Counting Enabled\n
   0 -  Disables the recording of SPI ISR cycle counting.
   1 -  Enables the recording of SPI ISR cycle counting.
*/
#define ADI_CYCLECOUNT_SPI_ISR_ENABLED                  (0u)


/*! CRC Interrupt Mode ISR Cycle Counting Enabled\n
   0 -  Disables the recording of CRC ISR cycle counting.
   1 -  Enables the recording of CRC ISR cycle counting.
*/
#define ADI_CYCLECOUNT_CRC_ISR_ENABLED                  (0u)


/*! SPORT Interrupt Mode ISR Cycle Counting Enabled\n
   0 -  Disables the recording of SPORT ISR cycle counting.
   1 -  Enables the recording of SPORT ISR cycle counting.
*/
#define ADI_CYCLECOUNT_SPORT_ISR_ENABLED                (0u)

/*! UART Interrupt Mode ISR Cycle Counting Enabled\n
   0 -  Disables the recording of UART ISR cycle counting.
   1 -  Enables the recording of UART ISR cycle counting.
*/
#define ADI_CYCLECOUNT_UART_ISR_ENABLED                 (0u)


/*! A user application may desire/require cycle counting in an application defined API
    or ISR. Set this macro to the number of required.
*/
#define ADI_CYCLECOUNT_NUMBER_USER_DEFINED_APIS         (0u)

/*!
 * Cycle count 'stack' nesting depth. Adjust as needed.
 * This should map to the maximum number of nested interrupts an application might experience.
 */
#define ADI_CYCLECOUNT_STACK_SIZE 10

/**
 *  @}
 */

#endif /* ADI_CYCLE_COUNTING_CONFIG_H */
