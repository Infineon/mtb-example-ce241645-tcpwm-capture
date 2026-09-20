/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for TCPWM Capture Mode Application Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/


/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Debug UART context */
cy_stc_scb_uart_context_t DEBUG_UART_context;

/* Debug UART HAL object */
mtb_hal_uart_t DEBUG_UART_hal_obj;

/* PPCA input selector configuration */
const cy_stc_ppca_cnfg_ppcaout_input_selector_t ppca_input_sel1 =
{
    .inputSelSrc = PPCAIN_SEL_SRC_0,
    .disSynchronizerStage = true,
};

/* Configure capture interrupt instance (capture or overflow) */
const cy_stc_sysint_t CAPTURE_IRQ_cfg =
{
    .intrSrc      = CAPTURE_IRQ,
    .intrPriority = 3UL
};

/* Capture period value */
uint32_t capture_period;

/* Capture duty value */
uint32_t capture_duty;

/* Captured frequency */
unsigned long capture_fre;

/* Capture signal flag */
uint32_t capture_signal = 0;

/* Calculated duty cycle */
float duty_cycle;


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void isr_tcpwm_capture(void);


/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function.
*  1. Configure a PWM: ~417 Hz and 25% duty cycle
*  2. Configure a counter as Capture mode to measure the frequency and duty cycle
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure GPIO pin */
    Cy_GPIO_Pin_FastInit(P9_0_PORT, P9_0_PIN, CY_GPIO_DM_HIGHZ, 1UL, P9_0_PPCA_PPSSIO6);

    /* Initialize the debug UART */
    result = Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Initialize HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config, &DEBUG_UART_context, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: TCPWM capture\r\n");
    printf("************************************************************\r\n\n");

    /* Enable PPCA */
    Cy_PPCA_Enable(CNFG_TCPWM_INOUT_HW);

    /* Configure PPCA output selector */
    Cy_PPCA_CNFG_PPCA_Output_Selector(CNFG_TCPWM_INOUTCNFG_HW, &CNFG_TCPWM_INOUT_ppcaOutConfig);

    /* Configure PPCA input selector */
    Cy_PPCA_CNFG_PPCA_Input_Selector(CNFG_TCPWM_INOUTCNFG_HW, &ppca_input_sel1, PPCA_INPUT_SRC_SELECT0);

    /* EPU configuration */
    Cy_PPCA_EPU_EnableExclusiveAccess(EPU_BLK_HW, true);
    Cy_PPCA_EPU_Enable(EPU_BLK_HW);

    /* Configure EPU processing units */
    Cy_PPCA_EPU_PU_T2_Configure(put2_0_HW, put2_0_INDEX, &put2_0_put2_config);
    Cy_PPCA_EPU_PU_T2_Enable(put2_0_HW, put2_0_INDEX, put2_0_ENABLE_MODE);
    Cy_PPCA_EPU_Combo_Configure(combiner12_HW, combiner12_INDEX, &combiner12_combo_config);

    /* Init and start PWM: ~417 Hz and 25% duty-cycle */
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_PWM_Init(PWM_HW, PWM_NUM, &PWM_config))
    {
        CY_ASSERT(0);
    }

    /* Enable the initialized PWM */
    Cy_TCPWM_PWM_Enable(PWM_HW, PWM_NUM);

    /* Then start the PWM */
    Cy_TCPWM_TriggerStart_Single(PWM_HW, PWM_NUM);

    /* TCPWM Capture Mode initial */
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(CAPTURE_HW, CAPTURE_NUM, &CAPTURE_config))
    {
        CY_ASSERT(0);
    }

    /* Enable the initialized counter */
    Cy_TCPWM_Counter_Enable(CAPTURE_HW, CAPTURE_NUM);

    /* Configure and enable interrupt */
    Cy_SysInt_Init(&CAPTURE_IRQ_cfg, isr_tcpwm_capture);
    NVIC_EnableIRQ(CAPTURE_IRQ_cfg.intrSrc);

    /* Enable global interrupts */
    __enable_irq();

    for (;;)
    {
        if (capture_signal)
        {
            /* Print captured value of input signal: frequency and duty cycle */
            printf("Frequency: %lu HZ\r\n", (long unsigned)capture_fre);
            printf("duty: %.3f %% \r\n", (double)duty_cycle);
            printf("\033[A");
            printf("\033[A");

            capture_signal = 0;
        }
    }
}


/*******************************************************************************
* Function Name: isr_tcpwm_capture
********************************************************************************
* Summary:
*  TCPWM capture interrupt handler. Reads the captured period and duty values,
*  calculates the frequency and duty cycle.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void isr_tcpwm_capture(void)
{
    /* Read interrupt trigger source */
    uint32_t status = Cy_TCPWM_Counter_GetStatus(CAPTURE_HW, CAPTURE_NUM);

    if (CY_TCPWM_INT_ON_CC0 & status)
    {
        /* Fetch the values of period and duty */
        capture_period = Cy_TCPWM_Counter_GetCapture0Val(CAPTURE_HW, CAPTURE_NUM);
        capture_duty = Cy_TCPWM_Counter_GetCapture1Val(CAPTURE_HW, CAPTURE_NUM);

        /* Calculate the frequency and duty cycle per the captured value.
         * The TCPWM counter clock (TCPWM_COUNTER_CLK) is 1 MHz. */
        capture_fre = 1000000 / capture_period;
        duty_cycle = ((float)capture_duty * 100) / (float)capture_period;

        capture_signal = 1;
    }

    /* Clear interrupt */
    Cy_TCPWM_ClearInterrupt(CAPTURE_HW, CAPTURE_NUM, status);
}


/* [] END OF FILE */
