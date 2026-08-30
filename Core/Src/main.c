/* USER CODE BEGIN Header */
/* USER CODE END Header */
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "bme280.h"
#include "lcd_i2c.h"
#include <stdio.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
typedef enum
{
    EKRAN_TEMPERATURA = 0,
    EKRAN_VLAGA,
    EKRAN_TLAK,
    EKRAN_BROJ
} Ekran_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PD */
#define TIPKALO_PORT          GPIOC
#define TIPKALO_PIN           GPIO_PIN_13
#define TIPKALO_AKTIVNO       GPIO_PIN_RESET

#define ODSKAKIVANJE_MS       40U
#define OSVJEZI_MS            1000U
/* USER CODE END PD */

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* USER CODE BEGIN PV */
static Zaslon_t  zaslon;
static Senzor_t  senzor;

static Ekran_t   ekran = EKRAN_TEMPERATURA;
static float     temperatura = 0.0f;
static float     tlak        = 0.0f;
static float     vlaga       = 0.0f;
static bool      senzor_radi = false;
/* USER CODE END PV */

void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void FormatirajBroj(float vrijednost, uint8_t decimala, char *izlaz, size_t velicina);
static void PrikaziEkran(void);
static bool TipkaloPritisnuto(void);
static void I2C_UkljuciInternePullUp(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

static void I2C_UkljuciInternePullUp(void)
{
    GPIO_InitTypeDef g = {0};

    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;

    g.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin       = GPIO_PIN_3 | GPIO_PIN_10;
    g.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOB, &g);
}

static void FormatirajBroj(float vrijednost, uint8_t decimala, char *izlaz, size_t velicina)
{
    const char *predznak = "";
    int32_t skala = 1;
    int32_t ukupno, cijeli, ostatak;

    if (vrijednost < 0.0f)
    {
        predznak = "-";
        vrijednost = -vrijednost;
    }

    for (uint8_t i = 0; i < decimala; i++)
        skala *= 10;

    ukupno  = (int32_t)(vrijednost * (float)skala + 0.5f);
    cijeli  = ukupno / skala;
    ostatak = ukupno % skala;

    if (decimala == 0)
        snprintf(izlaz, velicina, "%s%ld", predznak, (long)cijeli);
    else if (decimala == 1)
        snprintf(izlaz, velicina, "%s%ld.%01ld", predznak, (long)cijeli, (long)ostatak);
    else
        snprintf(izlaz, velicina, "%s%ld.%02ld", predznak, (long)cijeli, (long)ostatak);
}

static void PrikaziEkran(void)
{
    char broj[12];
    char redak[17];

    if (!senzor_radi)
    {
        LCD_IspisiRedak(&zaslon, 0, "Greska senzora!");
        LCD_IspisiRedak(&zaslon, 1, "Provjeri spoj");
        return;
    }

    switch (ekran)
    {
    case EKRAN_TEMPERATURA:
        FormatirajBroj(temperatura, 1, broj, sizeof(broj));
        snprintf(redak, sizeof(redak), "%s \xDF" "C", broj);
        LCD_IspisiRedak(&zaslon, 0, "Temperatura:");
        LCD_IspisiRedak(&zaslon, 1, redak);
        break;

    case EKRAN_VLAGA:
        FormatirajBroj(vlaga, 1, broj, sizeof(broj));
        snprintf(redak, sizeof(redak), "%s %%RH", broj);
        LCD_IspisiRedak(&zaslon, 0, "Vlaga:");
        LCD_IspisiRedak(&zaslon, 1, redak);
        break;

    case EKRAN_TLAK:
        FormatirajBroj(tlak, 1, broj, sizeof(broj));
        snprintf(redak, sizeof(redak), "%s hPa", broj);
        LCD_IspisiRedak(&zaslon, 0, "Tlak:");
        LCD_IspisiRedak(&zaslon, 1, redak);
        break;

    default:
        break;
    }
}

static bool TipkaloPritisnuto(void)
{
    static GPIO_PinState zadnje_stanje = GPIO_PIN_SET;
    static uint32_t zadnja_promjena = 0;
    static bool obradeno = true;

    GPIO_PinState sada = HAL_GPIO_ReadPin(TIPKALO_PORT, TIPKALO_PIN);

    if (sada != zadnje_stanje)
    {
        zadnje_stanje = sada;
        zadnja_promjena = HAL_GetTick();
        obradeno = false;
        return false;
    }

    if (!obradeno && (HAL_GetTick() - zadnja_promjena) > ODSKAKIVANJE_MS)
    {
        obradeno = true;
        if (sada == TIPKALO_AKTIVNO)
            return true;
    }

    return false;
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  uint32_t zadnje_mjerenje = 0;
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  I2C_UkljuciInternePullUp();

  if (LCD_Pokreni(&zaslon, &hi2c1) != HAL_OK)
      Error_Handler();

  LCD_IspisiRedak(&zaslon, 0, "Meteo stanica");
  LCD_IspisiRedak(&zaslon, 1, "Pokretanje...");
  HAL_Delay(1000);

  senzor_radi = (BME280_Pokreni(&senzor, &hi2c2) == HAL_OK);

  if (senzor_radi)
  {
      BME280_Ocitaj(&senzor, &temperatura, &tlak, &vlaga);
      zadnje_mjerenje = HAL_GetTick();
  }

  PrikaziEkran();
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (TipkaloPritisnuto())
      {
          ekran = (Ekran_t)((ekran + 1) % EKRAN_BROJ);
          PrikaziEkran();
      }

      if (senzor_radi && (HAL_GetTick() - zadnje_mjerenje) >= OSVJEZI_MS)
      {
          zadnje_mjerenje = HAL_GetTick();

          if (BME280_Ocitaj(&senzor, &temperatura, &tlak, &vlaga) != HAL_OK)
              senzor_radi = false;

          PrikaziEkran();
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif
