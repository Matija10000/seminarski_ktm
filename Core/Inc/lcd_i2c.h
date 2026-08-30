#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define LCD_ADRESA_PCF8574   (0x27 << 1)
#define LCD_ADRESA_PCF8574A  (0x3F << 1)

typedef struct
{
    I2C_HandleTypeDef *sabirnica;
    uint16_t adresa;
    uint8_t  svjetlo;
} Zaslon_t;

HAL_StatusTypeDef LCD_Pokreni(Zaslon_t *zaslon, I2C_HandleTypeDef *sabirnica);

void LCD_Obrisi(Zaslon_t *zaslon);
void LCD_PostaviKursor(Zaslon_t *zaslon, uint8_t red, uint8_t stupac);
void LCD_Ispisi(Zaslon_t *zaslon, const char *tekst);
void LCD_Svjetlo(Zaslon_t *zaslon, bool upaljeno);

void LCD_IspisiRedak(Zaslon_t *zaslon, uint8_t red, const char *tekst);

#endif
