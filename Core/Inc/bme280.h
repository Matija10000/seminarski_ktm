#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define BME280_ADRESA_NISKA   (0x76 << 1)
#define BME280_ADRESA_VISOKA  (0x77 << 1)

#define BME280_ID_CIPA         0x60

typedef struct
{
    I2C_HandleTypeDef *sabirnica;
    uint16_t adresa;

    uint16_t kal_T1;
    int16_t  kal_T2, kal_T3;
    uint16_t kal_P1;
    int16_t  kal_P2, kal_P3, kal_P4, kal_P5, kal_P6, kal_P7, kal_P8, kal_P9;
    uint8_t  kal_H1, kal_H3;
    int16_t  kal_H2, kal_H4, kal_H5;
    int8_t   kal_H6;

    int32_t  temp_baza;
} Senzor_t;

HAL_StatusTypeDef BME280_Pokreni(Senzor_t *senzor, I2C_HandleTypeDef *sabirnica);

HAL_StatusTypeDef BME280_Ocitaj(Senzor_t *senzor, float *temperatura, float *tlak, float *vlaga);

#endif
