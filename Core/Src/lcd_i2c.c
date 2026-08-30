#include "lcd_i2c.h"
#include <string.h>

#define ZASLON_SVJETLO  0x08
#define ZASLON_UPIS     0x04
#define ZASLON_SMJER    0x02
#define ZASLON_ODABIR   0x01

#define I2C_ISTEK_MS  100U

static void dwt_pokreni(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void pauza_us(uint32_t mikrosekundi)
{
    uint32_t pocetak_ciklusa = DWT->CYCCNT;
    uint32_t ciklusa = mikrosekundi * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - pocetak_ciklusa) < ciklusa) { __NOP(); }
}

static void pcf_pisi(Zaslon_t *zaslon, uint8_t bajt)
{
    HAL_I2C_Master_Transmit(zaslon->sabirnica, zaslon->adresa, &bajt, 1, I2C_ISTEK_MS);
}

static void lcd_posalji4(Zaslon_t *zaslon, uint8_t polubajt, uint8_t odabir)
{
    uint8_t d = (polubajt & 0xF0) | odabir | zaslon->svjetlo;

    pcf_pisi(zaslon, d | ZASLON_UPIS);
    pauza_us(1);
    pcf_pisi(zaslon, d & ~ZASLON_UPIS);
    pauza_us(50);
}

static void lcd_posalji(Zaslon_t *zaslon, uint8_t bajt, uint8_t odabir)
{
    lcd_posalji4(zaslon, bajt & 0xF0, odabir);
    lcd_posalji4(zaslon, (uint8_t)(bajt << 4), odabir);
}

static void lcd_naredba(Zaslon_t *zaslon, uint8_t naredba)
{
    lcd_posalji(zaslon, naredba, 0);
    if (naredba == 0x01 || naredba == 0x02)
        HAL_Delay(2);
}

static void lcd_podatak(Zaslon_t *zaslon, uint8_t bajt)
{
    lcd_posalji(zaslon, bajt, ZASLON_ODABIR);
}

HAL_StatusTypeDef LCD_Pokreni(Zaslon_t *zaslon, I2C_HandleTypeDef *sabirnica)
{
    const uint16_t kandidati[2] = { LCD_ADRESA_PCF8574, LCD_ADRESA_PCF8574A };
    bool nadeno = false;

    dwt_pokreni();

    zaslon->sabirnica = sabirnica;
    zaslon->svjetlo = ZASLON_SVJETLO;

    for (int i = 0; i < 2 && !nadeno; i++)
    {
        zaslon->adresa = kandidati[i];
        if (HAL_I2C_IsDeviceReady(sabirnica, zaslon->adresa, 3, I2C_ISTEK_MS) == HAL_OK)
            nadeno = true;
    }
    if (!nadeno)
        return HAL_ERROR;

    HAL_Delay(50);
    lcd_posalji4(zaslon, 0x30, 0); HAL_Delay(5);
    lcd_posalji4(zaslon, 0x30, 0); pauza_us(150);
    lcd_posalji4(zaslon, 0x30, 0); pauza_us(150);
    lcd_posalji4(zaslon, 0x20, 0); pauza_us(150);

    lcd_naredba(zaslon, 0x28);
    lcd_naredba(zaslon, 0x08);
    lcd_naredba(zaslon, 0x01);
    lcd_naredba(zaslon, 0x06);
    lcd_naredba(zaslon, 0x0C);

    return HAL_OK;
}

void LCD_Obrisi(Zaslon_t *zaslon)
{
    lcd_naredba(zaslon, 0x01);
}

void LCD_PostaviKursor(Zaslon_t *zaslon, uint8_t red, uint8_t stupac)
{
    static const uint8_t pocetak[2] = { 0x00, 0x40 };
    if (red > 1) red = 1;
    lcd_naredba(zaslon, 0x80 | (pocetak[red] + stupac));
}

void LCD_Ispisi(Zaslon_t *zaslon, const char *tekst)
{
    while (*tekst)
        lcd_podatak(zaslon, (uint8_t)*tekst++);
}

void LCD_IspisiRedak(Zaslon_t *zaslon, uint8_t red, const char *tekst)
{
    uint8_t n = 0;

    LCD_PostaviKursor(zaslon, red, 0);
    while (*tekst && n < 16)
    {
        lcd_podatak(zaslon, (uint8_t)*tekst++);
        n++;
    }
    while (n < 16)
    {
        lcd_podatak(zaslon, ' ');
        n++;
    }
}

void LCD_Svjetlo(Zaslon_t *zaslon, bool upaljeno)
{
    zaslon->svjetlo = upaljeno ? ZASLON_SVJETLO : 0x00;
    pcf_pisi(zaslon, zaslon->svjetlo);
}
