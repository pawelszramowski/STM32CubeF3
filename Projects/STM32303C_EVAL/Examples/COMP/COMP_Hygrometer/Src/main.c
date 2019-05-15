/**
  ******************************************************************************
  * @file    COMP/COMP_Hygrometer/Src/main.c 
  * @author  MCD Application Team
  * @brief   This example shows how to make a hygrometer using the capacitive
  *          humidity sensor HS1101.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stlogo.h"

/** @addtogroup STM32F3xx_HAL_Examples
  * @{
  */

/** @addtogroup COMP_Hygrometer
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef    Input_Handle;
TIM_HandleTypeDef    PWM_Handle;
COMP_HandleTypeDef   Comp4Handle;
DAC_HandleTypeDef    DacHandle;
static DAC_ChannelConfTypeDef sConfig;

__IO uint16_t AvrgICReadValue = 0;
uint16_t ICReadValue = 0;
uint16_t ICCounter = 0;
uint32_t SumICReadValue = 0;
uint16_t ICError = 0;
uint32_t DisplayValue = 50;
uint32_t UpdateDisplayValue = 1;
uint32_t RHmin = 99;
uint32_t RHmax = 0;
float Capacitance;
float TriggerTime;
float Capacitance55RH = 180e-12; /* typical capacitance value at 55% RH */
float RelativeHumidity;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Display_ExampleDescription(void);
static void HYGROMETER_SetHint(void);
static void Error_Handler(void);
static void DAC1_Config(void);
static void COMP4_Config(void);
static void TIM3_Config(void);
static void TIM4_Config(void);
static void Calibration_Menu(void);
static void Display_Humidity(uint32_t DisplayHumidity);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
int main(void)
{
  float capacitanceratio;

  /* STM32F3xx HAL library initialization:
       - Configure the Flash prefetch
       - Systick timer is configured by default as source of time base, but user 
         can eventually implement his proper time base source (a general purpose 
         timer for example or other time source), keeping in mind that Time base 
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and 
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();

  /* Configure the system clock to have a system clock = 72 Mhz */
  SystemClock_Config();

  /******* Initialize LEDs available on STM32303C-EVAL board ******************/
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);

  /*##-1- Initialize the Key and Joystick ####################################*/
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
  BSP_JOY_Init(JOY_MODE_GPIO);
  
  /*##-2- Initialize the LCD #################################################*/
  /* Initialize the LCD */
  BSP_LCD_Init();
   
  /*##-3- Display messages on LCD ############################################*/  
  /* Display Example Welcome message */
  Display_ExampleDescription();

  /* Wait For User inputs */
  while (1)
  {
    if (BSP_PB_GetState(BUTTON_KEY) == RESET)
    {
      while (BSP_PB_GetState(BUTTON_KEY) == RESET);
      break;
    }
  }
  
  /* Display Example Template */
  HYGROMETER_SetHint();
  
  /*##-4- Configure DAC1 #####################################################*/  
  /* configure DAC1 */
  DAC1_Config();

  /*##-5- Configure Comparators ##############################################*/  
  /* configure COMP4 */
  COMP4_Config();

  /*##-6- Configure Timers ###################################################*/  
  TIM3_Config();
  TIM4_Config();
  
  /*##-7- Start Example ######################################################*/  
  /* wait until first AvrgICReadValue is calculated */
  while(AvrgICReadValue == 0);
  
  /* Enter Calibration menu */
  Calibration_Menu();
  
  /* Infinite loop */
  while (1)
  {
    /* Calculate Trigger Time Value */
    TriggerTime = (float) (AvrgICReadValue-ICError)/SystemCoreClock;

    /* Comp4 inverted input connected to DAC1 :
     * TriggerTime = RES * Capacitance * ln(VDD/(VDD - VREF))
     * @VREF = 2.086V (generated by DAC),  ln(VDD/(VDD - VREF)) is ~ 1
     *  ==>  Capacitance = TriggerTime/RES
     */
    Capacitance = (float) TriggerTime/RES;
    
/* Calculate humidity value using reversed polynomial expression */
    capacitanceratio = Capacitance/Capacitance55RH;
    /* RH (%) = -3.4656*10^3 * X^3 + 1.0732*10^4 * X^2 - 1.0457*10^4*X + 3.2459*10^3
       with X = C (read) / C@55%RH = capacitanceratio */
    RelativeHumidity = RP3 * pow(capacitanceratio, 3) + 
                       RP2 * pow(capacitanceratio, 2) + 
                       RP1 * capacitanceratio + 
                       RP0;

    /* Restrict Relative Humidity Value to 0-99 Domain */
    if (RelativeHumidity < 0)
    {
      RelativeHumidity = 0;
    }
    if (RelativeHumidity > 99)
    {
      RelativeHumidity = 99;
    }
    
    /* Display the humidity value */
    Display_Humidity((uint32_t) RelativeHumidity);
  }
}

/**
  * @brief  Display main example message
  * @param  None
  * @retval None
  */
static void Display_ExampleDescription(void)
{
  BSP_LCD_SetFont(&LCD_DEFAULT_FONT);
  
  /* Clear the LCD */ 
  BSP_LCD_SetBackColor(LCD_COLOR_WHITE); 
  BSP_LCD_Clear(LCD_COLOR_WHITE);

  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);  

  /* Display LCD messages */
  BSP_LCD_DisplayStringAt(0, 10, (uint8_t *)"STM32F303C", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 35, (uint8_t *)"Example", CENTER_MODE);
  /* Draw Bitmap */
  BSP_LCD_DrawBitmap((BSP_LCD_GetXSize() - 80)/2, 65, (uint8_t *)stlogo);
  
  BSP_LCD_SetFont(&Font12);
  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize()- 20, (uint8_t *)"Copyright (c) STMicroelectronics 2016", CENTER_MODE);
  
  BSP_LCD_SetFont(&Font16);
  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);
  BSP_LCD_FillRect(0, BSP_LCD_GetYSize()/2 + 15, BSP_LCD_GetXSize(), 60);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKBLUE); 
  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize()/2 + 30, (uint8_t *)"Press Key Button to start :", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize()/2 + 45, (uint8_t *)" HYGROMETER Example", CENTER_MODE);
}

/**
  * @brief  Display Hygrometer Example Hint
  * @param  None
  * @retval None
  */
static void HYGROMETER_SetHint(void)
{
  /* Clear the LCD */ 
  BSP_LCD_Clear(LCD_COLOR_WHITE);
  
  /* Set LCD Demo description */
  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);
  BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 80);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKBLUE); 
  BSP_LCD_SetFont(&Font24);
  BSP_LCD_DisplayStringAt(0, 0, (uint8_t*)"HYGROMETER EXAMPLE", CENTER_MODE);
  BSP_LCD_SetFont(&Font12);
  BSP_LCD_DisplayStringAt(0, 30, (uint8_t*)"This example shows how to make", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 45, (uint8_t*)"a hygrometer using", CENTER_MODE); 
  BSP_LCD_DisplayStringAt(0, 60, (uint8_t*)"the capacitive humidity sensor HS1101", CENTER_MODE); 

  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);  
  BSP_LCD_DrawRect(10, 90, BSP_LCD_GetXSize()- 20, BSP_LCD_GetYSize() - 100);
  BSP_LCD_DrawRect(11, 91, BSP_LCD_GetXSize()- 22, BSP_LCD_GetYSize() - 102);
 }


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *=============================================================================
  *=============================================================================
  *                         Supported STM32F30x device                          
  *-----------------------------------------------------------------------------
  *        System Clock source                    | PLL (HSE)
  *-----------------------------------------------------------------------------
  *        SYSCLK(Hz)                             | 72000000
  *-----------------------------------------------------------------------------
  *        HCLK(Hz)                               | 72000000
  *-----------------------------------------------------------------------------
  *        AHB Prescaler                          | 1
  *-----------------------------------------------------------------------------
  *        APB2 Prescaler                         | 2
  *-----------------------------------------------------------------------------
  *        APB1 Prescaler                         | 2
  *-----------------------------------------------------------------------------
  *        HSE Frequency(Hz)                      | 8000000
  *----------------------------------------------------------------------------
  *        PLLMUL                                 | 9
  *-----------------------------------------------------------------------------
  *        PREDIV                                 | 1
  *-----------------------------------------------------------------------------
  *        USB Clock                              | DISABLE
  *-----------------------------------------------------------------------------
  *        Flash Latency(WS)                      | 2
  *-----------------------------------------------------------------------------
  *        Prefetch Buffer                        | OFF
  *-----------------------------------------------------------------------------
  *=============================================================================
  ******************************************************************************
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
#ifdef USE_FULL_ASSERT
  uint32_t ret = HAL_OK;
#endif /* USE_FULL_ASSERT */

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

#ifdef USE_FULL_ASSERT
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    assert_failed((char *)__FILE__, __LINE__);
  }
#else
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
#endif /* USE_FULL_ASSERT */

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

#ifdef USE_FULL_ASSERT
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
  if(ret != HAL_OK)
  {
    assert_failed((char *)__FILE__, __LINE__);
  }
#else
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
#endif /* USE_FULL_ASSERT */
}
/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
    /* Turn LED3 on */
    BSP_LED_On(LED3);
    while(1)
    {
    }
}

/**
  * @brief  Configure DAC1
  * @param  None
  * @retval None
  */
static void DAC1_Config(void)
{
  /*##-1- Configure the DAC peripheral #######################################*/
  DacHandle.Instance = DAC1;
  
  if(HAL_DAC_Init(&DacHandle) != HAL_OK)
  {
    /* Initiliazation Error */
    Error_Handler();    
  }

  /*##-2- Configure DAC channel1 #############################################*/  
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;  
  
  if(HAL_DAC_ConfigChannel(&DacHandle, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    /* Channel configuration Error */
    Error_Handler();    
  }
    
  /*##-3- Set DAC Channel1 DHR register ######################################*/ 
  /* VREF ~ 3.3 V                                                             */
  /* DAC_OUT = (3.3 * 2588) / 4095 ~ 2.086 V = 3.3*(1-exp(-t/R*C)) with t=R*C */
  /* Set DAC Value = 2588                                                     */
  if(HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2588) != HAL_OK)
  {
    /* Setting value Error */
    Error_Handler();        
  }
  
  /*##-4- Enable DAC Channel1 ################################################*/ 
  if(HAL_DAC_Start(&DacHandle, DAC_CHANNEL_1) != HAL_OK)
  {
    /* Start Error */
    Error_Handler();      
  }
}

/**
  * @brief  Configure COMP4
  * @param  None
  * @retval None
  */
static void COMP4_Config(void)
{
  /*##-1- Configure the COMPx peripheral ###################################*/
  /* COMP4 Init: Inverted Pin is connected to DAC1 */
  /* Non-inverted pin is connected to Humidity Sensor */
  Comp4Handle.Instance = COMP4;

  Comp4Handle.Init.InvertingInput = COMP_INVERTINGINPUT_DAC1;
  Comp4Handle.Init.NonInvertingInput = COMP_NONINVERTINGINPUT_IO1;
  Comp4Handle.Init.Output = COMP_OUTPUT_TIM4IC2;
  Comp4Handle.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
  Comp4Handle.Init.Hysteresis = COMP_HYSTERESIS_NONE;
  Comp4Handle.Init.BlankingSrce = COMP_BLANKINGSRCE_NONE;
  Comp4Handle.Init.Mode = COMP_MODE_HIGHSPEED;
  Comp4Handle.Init.WindowMode = COMP_WINDOWMODE_DISABLE;
  Comp4Handle.Init.TriggerMode = COMP_TRIGGERMODE_NONE;
  HAL_COMP_Init(&Comp4Handle);

  /*##-2- Start the comparator process #####################################*/ 
  /* Enable COMP4 */
  HAL_COMP_Start(&Comp4Handle);
}

/**             
  * @brief  TIM3 Configuration
  * @note   TIM3 configuration is based on APB1 frequency
  * @note   TIM3 Update event occurs each SystemCoreClock/FREQ   
  * @param  None
  * @retval None
  */
void TIM3_Config(void)
{
  TIM_OC_InitTypeDef    TIMPWM_Config;
  uint16_t PeriodValue = 0;

  TIM_MasterConfigTypeDef sMasterConfig;
  
  /*##-1- Configure the TIM peripheral #######################################*/
  /* PWM configuration */
  PWM_Handle.Instance = TIM3;
  
  /* Calculate the period value */
  PeriodValue = (uint16_t) ((SystemCoreClock) / FREQ);
  
  PWM_Handle.Init.Period = PeriodValue;          
  PWM_Handle.Init.Prescaler = 0;       
  PWM_Handle.Init.ClockDivision = 0;    
  PWM_Handle.Init.CounterMode = TIM_COUNTERMODE_UP; 
  PWM_Handle.Init.RepetitionCounter = 0;
  PWM_Handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  HAL_TIM_PWM_Init(&PWM_Handle);

  /*##-2- Configure the PWM Output Capture ########################################*/  
  /* PWM Output Capture configuration of channel 3 */
  TIMPWM_Config.OCMode  = TIM_OCMODE_PWM2;
  TIMPWM_Config.Pulse = PeriodValue/2;
  TIMPWM_Config.OCPolarity = TIM_OCPOLARITY_LOW;
  TIMPWM_Config.OCFastMode = TIM_OCFAST_DISABLE;
  TIMPWM_Config.OCIdleState = TIM_OCIDLESTATE_SET;
  if(HAL_TIM_PWM_ConfigChannel(&PWM_Handle, &TIMPWM_Config, TIM_CHANNEL_3) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /*##-3- Configure the Trigger Output #######################################*/  
  /* TIM3 TRGO selection */
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  HAL_TIMEx_MasterConfigSynchronization(&PWM_Handle, &sMasterConfig);

  /* Reset the flags */
  PWM_Handle.Instance->SR = 0;

  /*##-4- Enable TIM peripheral counter ######################################*/
  /* Start the TIM Output Capture measurement in interrupt mode */
  if(HAL_TIM_PWM_Start(&PWM_Handle, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**             
  * @brief  TIM4 Configuration
  * @note   TIM4 configuration is based on APB1 frequency
  * @note   TIM4 Update event occurs each Input Capture Rising Edge   
  * @param  None
  * @retval None
  */
void TIM4_Config(void)
{
  TIM_IC_InitTypeDef    TIMInput_Config;
  uint16_t PeriodValue = 0;

  TIM_SlaveConfigTypeDef sSlaveConfig;
  
  /*##-1- Configure the TIM peripheral #######################################*/
  /* Input Capture configuration */
  Input_Handle.Instance = TIM4;
  
  /* Calculate the period value */
  PeriodValue = (uint16_t) ((SystemCoreClock) / FREQ);
  
  Input_Handle.Init.Period = PeriodValue;          
  Input_Handle.Init.Prescaler = 0;       
  Input_Handle.Init.ClockDivision = 0;    
  Input_Handle.Init.CounterMode = TIM_COUNTERMODE_UP; 
  Input_Handle.Init.RepetitionCounter = 0;
  Input_Handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_IC_Init(&Input_Handle);

  /*##-2- Configure the Input Capture ########################################*/  
  /* Input Capture configuration of channel 2 */
  TIMInput_Config.ICPolarity  = TIM_ICPOLARITY_RISING;
  TIMInput_Config.ICSelection = TIM_ICSELECTION_DIRECTTI;
  TIMInput_Config.ICPrescaler = TIM_ICPSC_DIV1;
  TIMInput_Config.ICFilter    = 0;
  if(HAL_TIM_IC_ConfigChannel(&Input_Handle, &TIMInput_Config, TIM_CHANNEL_2) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler();
  }

  /*##-3- Configure the Trigger Input ########################################*/  
  /* TIM4 Input Trigger selection */
  sSlaveConfig.InputTrigger = TIM_TS_ITR2;
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_TRIGGER;

  HAL_TIM_SlaveConfigSynchronization(&Input_Handle, &sSlaveConfig);

  /* Reset the flags */
  Input_Handle.Instance->SR = 0;

  /*##-4- Enable TIM peripheral counter ######################################*/
  /* Start the TIM Input Capture measurement in interrupt mode */
  if(HAL_TIM_IC_Start_IT(&Input_Handle, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Input Capture callback in non blocking mode 
  * @param  htim : TIM4 IC handle
  * @retval None
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    /* Get the converted value */
    ICReadValue = HAL_TIM_ReadCapturedValue(htim,TIM_CHANNEL_2);
    
    /* Increment the counter */
    ICCounter++;
    
    /* sum up the samples */
    SumICReadValue += ICReadValue;
    
    if(ICCounter > 255)
    {
      /* Compute the average value for 256 samples */
      AvrgICReadValue = SumICReadValue/256;
      ICCounter = 0;
      SumICReadValue = 0;
    }
}

/**
  * @brief  Enter Calibration menu to correct ICError and Capacitance55RH values
  * @param  None
  * @retval None 
  */
static void Calibration_Menu(void)
{
  uint8_t exitmenu = 0;
  uint8_t LCDstr[20] = {0};
  
  /*##-1- Display messages on LCD ############################################*/  
  /* Set the LCD Text Color */
  BSP_LCD_SetBackColor(LCD_COLOR_WHITE); 
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
  
  /* Display Calibration Screen */
  BSP_LCD_DisplayStringAt(0, 115, (uint8_t*)"Calibration steps: ", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 145, (uint8_t *)"  1. Set JP19 to REF  ", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 160, (uint8_t *)"  2. Press SEL button ", CENTER_MODE);

  /*##-2- Calibration Phases #################################################*/  
  /* ------------- First step calibration using reference capacitance ----- */
  while (exitmenu != 1)
  { 
    if (BSP_JOY_GetState() == JOY_SEL) 
    {
      /* Get ICError for reference capacitance */
      /* TriggerTime = (AvrgICReadValue - ICError)/SystemCoreClock
      * TriggerTime = RES * REFCAP * ln(VDD/(VDD - VREF))
      * @VREF = 2.086V (generated by DAC),  ln(VDD/(VDD - VREF)) is ~ 1
      *  ==> TriggerTime = RES * REFCAP
      *  Then RES * REFCAP = (AvrgICReadValue - ICError)/SystemCoreClock
      *  ==>  ICError = AvrgICReadValue - REFCAP * RES * SystemCoreClock
      */
      ICError = (uint16_t) (AvrgICReadValue-REFCAP*RES*SystemCoreClock);

      /* Set exitmenu to 1 */
      exitmenu = 1;
    }
  }

  /* --------------- Second step calibration using reference humidity ------- */
  exitmenu = 0;
  /* Clear Calibration Screen */
  BSP_LCD_SetBackColor(LCD_COLOR_WHITE); 
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);    
  BSP_LCD_FillRect(12, 92, BSP_LCD_GetXSize()- 24, BSP_LCD_GetYSize() - 104);

  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);    

  BSP_LCD_DisplayStringAt(0, 100, (uint8_t *)"Now set JP19 to HUM ", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 115, (uint8_t *)"and set current humidity value", CENTER_MODE);
  BSP_LCD_DisplayStringAt(0, 130, (uint8_t *)"using Up/Down keys", CENTER_MODE);
  BSP_LCD_DisplayStringAt(14, 190, (uint8_t *)"Sel Button: Apply   ", LEFT_MODE);
  BSP_LCD_DisplayStringAt(14, 205, (uint8_t *)"Key Button: Cancel  ", LEFT_MODE);
  
  /* Set the LCD Text Color for Humidity value */
  BSP_LCD_SetTextColor(LCD_COLOR_RED);  

  /* Wait for SEL button to be released */
  while (BSP_JOY_GetState() != JOY_NONE); 
    
  while (exitmenu != 1)
  {
    if ((BSP_JOY_GetState() == JOY_UP) && (DisplayValue<99))
    {
      /* Wait for UP button to be released */
      while (BSP_JOY_GetState() != JOY_NONE);
      DisplayValue++;
      UpdateDisplayValue = 1;
    }
    if ((BSP_JOY_GetState() == JOY_DOWN) && (DisplayValue>0))
    {
      /* Wait for DOWN button to be released */
      while (BSP_JOY_GetState() != JOY_NONE);
      DisplayValue--;
      UpdateDisplayValue = 1;
    }
    
    if (UpdateDisplayValue)
    {
      UpdateDisplayValue = 0;
      /* Display humidity value on LCD Line 4 */ 
      sprintf((char*)LCDstr, "        %lu %%       ", DisplayValue);
      BSP_LCD_DisplayStringAt(0, 160, (uint8_t*) LCDstr, CENTER_MODE);
    }

    if (BSP_JOY_GetState() == JOY_SEL)
    {
      /* Calculate Trigger Time Value */
      TriggerTime = (float) (AvrgICReadValue-ICError)/SystemCoreClock;
      
      /* Calculate Capacitance Value */
      Capacitance = (float) TriggerTime/RES;
      
      /* Update Capacitance55RH value: capacitance @ 55% Relative Humidity */
      Capacitance55RH= Capacitance/(P3 * pow(DisplayValue,3) + 
                                    P2 * pow(DisplayValue,2) + 
                                    P1 * DisplayValue + 
                                    P0 );
      exitmenu = 1;
    }
    else if (BSP_PB_GetState(BUTTON_KEY) != SET) 
    {
      /* Humidity Calibration canceled */
      exitmenu = 1; 
    }
  }

  /* Clear Calibration Screen */
  BSP_LCD_SetBackColor(LCD_COLOR_WHITE);  
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);    
  BSP_LCD_FillRect(12, 92, BSP_LCD_GetXSize()- 24, BSP_LCD_GetYSize() - 104);

  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);    
}

/**
  * @brief  Calculate and Dispaly the humidity value on LCD
  * @param  DisplayValue : Relative Humidity in percent
  * @retval None
  */
static void Display_Humidity(uint32_t DisplayHumidity)
{
  uint8_t LCDstr[20] = {0};
  
  /* Display humidity value on LCD */ 
  BSP_LCD_DisplayStringAt(0, 115, (uint8_t *)"Humidity Measured :", CENTER_MODE);
  sprintf((char*)LCDstr, "        %lu %%        ", DisplayHumidity);
  BSP_LCD_DisplayStringAt(0, 145, (uint8_t*) LCDstr, CENTER_MODE);
  
  /* Store the minimal and maximal RH values */
  if (RelativeHumidity < RHmin)
  {
    RHmin = (uint8_t) RelativeHumidity;
  }
  
  if (RelativeHumidity > RHmax)
  {
    RHmax = (uint8_t) RelativeHumidity;
  }
  
  /* Display humidity range history on LCD */ 
  BSP_LCD_DisplayStringAt(0, 190, (uint8_t *)"Humidity History :", CENTER_MODE);
  sprintf((char*)LCDstr, " Min= %lu%%    Max= %lu%% ", RHmin, RHmax);
  BSP_LCD_DisplayStringAt(0, 205, (uint8_t *) LCDstr, CENTER_MODE);
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/