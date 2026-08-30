#include "bme280.h"

#define REG_KALIBRACIJA_A      0x88
#define REG_OZNAKA             0xD0
#define REG_PONOVO_POKRENI     0xE0
#define REG_KALIBRACIJA_B      0xE1
#define REG_POSTAVKE_VLAGE     0xF2
#define REG_ZAUZETOST          0xF3
#define REG_POSTAVKE_MJERENJA  0xF4
#define REG_POSTAVKE_CIKLUSA   0xF5
#define REG_REZULTATI          0xF7

#define I2C_ISTEK_MS    100U

static HAL_StatusTypeDef bme_citaj(Senzor_t *senzor, uint8_t registar, uint8_t *spremnik, uint16_t duljina)
{
    return HAL_I2C_Mem_Read(senzor->sabirnica, senzor->adresa, registar, I2C_MEMADD_SIZE_8BIT, spremnik, duljina, I2C_ISTEK_MS);
}

static HAL_StatusTypeDef bme_pisi(Senzor_t *senzor, uint8_t registar, uint8_t vrijednost)
{
    return HAL_I2C_Mem_Write(senzor->sabirnica, senzor->adresa, registar, I2C_MEMADD_SIZE_8BIT, &vrijednost, 1, I2C_ISTEK_MS);
}

static HAL_StatusTypeDef bme_citaj_kalibraciju(Senzor_t *senzor)
{
    uint8_t c[26];
    uint8_t h[7];

    if (bme_citaj(senzor, REG_KALIBRACIJA_A, c, 26) != HAL_OK)
        return HAL_ERROR;

    senzor->kal_T1 = (uint16_t)(c[1]  << 8 | c[0]);
    senzor->kal_T2 = (int16_t) (c[3]  << 8 | c[2]);
    senzor->kal_T3 = (int16_t) (c[5]  << 8 | c[4]);
    senzor->kal_P1 = (uint16_t)(c[7]  << 8 | c[6]);
    senzor->kal_P2 = (int16_t) (c[9]  << 8 | c[8]);
    senzor->kal_P3 = (int16_t) (c[11] << 8 | c[10]);
    senzor->kal_P4 = (int16_t) (c[13] << 8 | c[12]);
    senzor->kal_P5 = (int16_t) (c[15] << 8 | c[14]);
    senzor->kal_P6 = (int16_t) (c[17] << 8 | c[16]);
    senzor->kal_P7 = (int16_t) (c[19] << 8 | c[18]);
    senzor->kal_P8 = (int16_t) (c[21] << 8 | c[20]);
    senzor->kal_P9 = (int16_t) (c[23] << 8 | c[22]);
    senzor->kal_H1 = c[25];

    if (bme_citaj(senzor, REG_KALIBRACIJA_B, h, 7) != HAL_OK)
        return HAL_ERROR;

    senzor->kal_H2 = (int16_t)(h[1] << 8 | h[0]);
    senzor->kal_H3 = h[2];
    senzor->kal_H4 = (int16_t)((int8_t)h[3] * 16 + (h[4] & 0x0F));
    senzor->kal_H5 = (int16_t)((int8_t)h[5] * 16 + (h[4] >> 4));
    senzor->kal_H6 = (int8_t)h[6];

    return HAL_OK;
}

HAL_StatusTypeDef BME280_Pokreni(Senzor_t *senzor, I2C_HandleTypeDef *sabirnica)
{
    const uint16_t kandidati[2] = { BME280_ADRESA_NISKA, BME280_ADRESA_VISOKA };
    uint8_t ocitani_id = 0;
    bool nadeno = false;

    senzor->sabirnica = sabirnica;
    senzor->temp_baza = 0;

    for (int i = 0; i < 2 && !nadeno; i++)
    {
        senzor->adresa = kandidati[i];
        if (HAL_I2C_IsDeviceReady(sabirnica, senzor->adresa, 3, I2C_ISTEK_MS) != HAL_OK)
            continue;
        if (bme_citaj(senzor, REG_OZNAKA, &ocitani_id, 1) != HAL_OK)
            continue;
        if (ocitani_id == BME280_ID_CIPA)
            nadeno = true;
    }
    if (!nadeno)
        return HAL_ERROR;

    if (bme_pisi(senzor, REG_PONOVO_POKRENI, 0xB6) != HAL_OK)
        return HAL_ERROR;
    HAL_Delay(10);

    for (int i = 0; i < 20; i++)
    {
        uint8_t stanje;
        if (bme_citaj(senzor, REG_ZAUZETOST, &stanje, 1) == HAL_OK && !(stanje & 0x01))
            break;
        HAL_Delay(5);
    }

    if (bme_citaj_kalibraciju(senzor) != HAL_OK)
        return HAL_ERROR;

    bme_pisi(senzor, REG_POSTAVKE_VLAGE, 0x01);

    if (bme_pisi(senzor, REG_POSTAVKE_CIKLUSA, 0xA0) != HAL_OK)
        return HAL_ERROR;
    if (bme_pisi(senzor, REG_POSTAVKE_MJERENJA, 0x27) != HAL_OK)
        return HAL_ERROR;

    HAL_Delay(100);
    return HAL_OK;
}

static int32_t kompenziraj_temperaturu(Senzor_t *senzor, int32_t sirova_temperatura)
{
    int32_t pom1, pom2;

    pom1 = ((((sirova_temperatura >> 3) - ((int32_t)senzor->kal_T1 << 1))) * ((int32_t)senzor->kal_T2)) >> 11;
    pom2 = (((((sirova_temperatura >> 4) - ((int32_t)senzor->kal_T1)) *
              ((sirova_temperatura >> 4) - ((int32_t)senzor->kal_T1))) >> 12) * ((int32_t)senzor->kal_T3)) >> 14;

    senzor->temp_baza = pom1 + pom2;
    return (senzor->temp_baza * 5 + 128) >> 8;
}

static uint32_t kompenziraj_tlak(Senzor_t *senzor, int32_t sirovi_tlak)
{
    int64_t pom1, pom2, rezultat;

    pom1 = ((int64_t)senzor->temp_baza) - 128000;
    pom2 = pom1 * pom1 * (int64_t)senzor->kal_P6;
    pom2 = pom2 + ((pom1 * (int64_t)senzor->kal_P5) << 17);
    pom2 = pom2 + (((int64_t)senzor->kal_P4) << 35);
    pom1 = ((pom1 * pom1 * (int64_t)senzor->kal_P3) >> 8) + ((pom1 * (int64_t)senzor->kal_P2) << 12);
    pom1 = (((((int64_t)1) << 47) + pom1)) * ((int64_t)senzor->kal_P1) >> 33;

    if (pom1 == 0)
        return 0;

    rezultat = 1048576 - sirovi_tlak;
    rezultat = (((rezultat << 31) - pom2) * 3125) / pom1;
    pom1 = (((int64_t)senzor->kal_P9) * (rezultat >> 13) * (rezultat >> 13)) >> 25;
    pom2 = (((int64_t)senzor->kal_P8) * rezultat) >> 19;
    rezultat = ((rezultat + pom1 + pom2) >> 8) + (((int64_t)senzor->kal_P7) << 4);

    return (uint32_t)rezultat;
}

static uint32_t kompenziraj_vlagu(Senzor_t *senzor, int32_t sirova_vlaga)
{
    int32_t rezultat;

    rezultat = senzor->temp_baza - ((int32_t)76800);
    rezultat = (((((sirova_vlaga << 14) - (((int32_t)senzor->kal_H4) << 20) - (((int32_t)senzor->kal_H5) * rezultat)) +
           ((int32_t)16384)) >> 15) *
         (((((((rezultat * ((int32_t)senzor->kal_H6)) >> 10) *
              (((rezultat * ((int32_t)senzor->kal_H3)) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)senzor->kal_H2) + 8192) >> 14));
    rezultat = rezultat - (((((rezultat >> 15) * (rezultat >> 15)) >> 7) * ((int32_t)senzor->kal_H1)) >> 4);
    if (rezultat < 0)         rezultat = 0;
    if (rezultat > 419430400) rezultat = 419430400;

    return (uint32_t)(rezultat >> 12);
}

HAL_StatusTypeDef BME280_Ocitaj(Senzor_t *senzor, float *temperatura, float *tlak, float *vlaga)
{
    uint8_t sirovi[8];
    int32_t sirovi_tlak, sirova_temperatura, sirova_vlaga;
    int32_t temp_stotinke;

    if (bme_citaj(senzor, REG_REZULTATI, sirovi, 8) != HAL_OK)
        return HAL_ERROR;

    sirovi_tlak = ((int32_t)sirovi[0] << 12) | ((int32_t)sirovi[1] << 4) | (sirovi[2] >> 4);
    sirova_temperatura = ((int32_t)sirovi[3] << 12) | ((int32_t)sirovi[4] << 4) | (sirovi[5] >> 4);

    temp_stotinke = kompenziraj_temperaturu(senzor, sirova_temperatura);
    if (temperatura) *temperatura = (float)temp_stotinke / 100.0f;

    if (tlak)
    {
        uint32_t rezultat = kompenziraj_tlak(senzor, sirovi_tlak);
        *tlak = (float)rezultat / 25600.0f;
    }

    if (vlaga)
    {
        sirova_vlaga = ((int32_t)sirovi[6] << 8) | sirovi[7];
        *vlaga = (float)kompenziraj_vlagu(senzor, sirova_vlaga) / 1024.0f;
    }

    return HAL_OK;
}

