#include "stm8l15x.h"
#include "stm8l15x_conf.h"
#include <string.h>
#include <ctype.h>

/* ---------------- CONFIG ---------------- */
#define I2C_INTERFACE        I2C1
#define I2C_CLK              CLK_Peripheral_I2C1
#define STM8_SLAVE_ADDR      (0x27 << 1)
#define BUFFER_SIZE          64

/* ---------------- RGB LED Mapping ---------------- */
#define RGB_A_PORT GPIOD
#define RGB_A_PIN  GPIO_Pin_3
#define RGB_B_PORT GPIOD
#define RGB_B_PIN  GPIO_Pin_4
#define RGB_C_PORT GPIOD
#define RGB_C_PIN  GPIO_Pin_5

/* ---------------- Relay Pin Mapping ---------------- */
#define RELAY_C_PORT GPIOD
#define RELAY_C_PIN  GPIO_Pin_7
#define RELAY_D_PORT GPIOD
#define RELAY_D_PIN  GPIO_Pin_6
#define RELAY_E_PORT GPIOE
#define RELAY_E_PIN  GPIO_Pin_0
#define RELAY_F_PORT GPIOG
#define RELAY_F_PIN  GPIO_Pin_3
#define RELAY_G_PORT GPIOA
#define RELAY_G_PIN  GPIO_Pin_4
#define RELAY_H_PORT GPIOA
#define RELAY_H_PIN  GPIO_Pin_5

/* ---------------- AC Contractor Mapping ---------------- */
#define AC_CONTRACTOR1_PORT GPIOF
#define AC_CONTRACTOR1_PIN  GPIO_Pin_6
#define AC_CONTRACTOR2_PORT GPIOF
#define AC_CONTRACTOR2_PIN  GPIO_Pin_5

/* ---------------- Rectifier Inputs (10 + 4 extra) ---------------- */
/*
   1 -> PD0
   2 -> PE5
   3 -> PE4
   4 -> PE3
   5 -> PE2
   6 -> PE1
   7 -> PC7
   8 -> PE6
   9 -> PC6
  10 -> PC5
  11 -> PB7 (UV/OV)
  12 -> PB6 (Door Latch)
  13 -> PB2 (Contactor FB)
  14 -> PC4 (ELR Detect)
*/
GPIO_TypeDef* rect_ports[15] = {
    GPIOD, GPIOE, GPIOE, GPIOE, GPIOE, GPIOE,
    GPIOC, GPIOE, GPIOC, GPIOC,
    GPIOB, GPIOB, GPIOB, GPIOC, GPIOD
};

uint8_t rect_pins[15] = {
    GPIO_Pin_0, GPIO_Pin_5, GPIO_Pin_4, GPIO_Pin_3, GPIO_Pin_2, GPIO_Pin_1,
    GPIO_Pin_7, GPIO_Pin_6, GPIO_Pin_6, GPIO_Pin_5,
    GPIO_Pin_7, GPIO_Pin_6, GPIO_Pin_2, GPIO_Pin_4, GPIO_Pin_1
};

/* ---------------- VARIABLES ---------------- */
volatile uint8_t relay_status = 0x00;
volatile uint8_t rgb_status = 0x00;
volatile uint8_t ac_status = 0x00;

char i2c_rx_buffer[BUFFER_SIZE];
uint8_t rx_index = 0;
char status_msg[64];

/* ---------------- UART ---------------- */
void uart_init(void) {
    CLK_PeripheralClockConfig(CLK_Peripheral_USART1, ENABLE);
    USART_Init(USART1, 115200, USART_WordLength_8b, USART_StopBits_1,
               USART_Parity_No, USART_Mode_Tx | USART_Mode_Rx);
    USART_Cmd(USART1, ENABLE);
}

void UART_TX_String(const char *str) {
    while (*str) {
        while (!USART_GetFlagStatus(USART1, USART_FLAG_TXE));
        USART_SendData8(USART1, *str++);
    }
    while (!USART_GetFlagStatus(USART1, USART_FLAG_TC));
}

/* ---------------- GPIO ---------------- */
void gpio_init(void) {
    uint8_t i;

    /* RGB LEDs */
    GPIO_Init(RGB_A_PORT, RGB_A_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RGB_B_PORT, RGB_B_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RGB_C_PORT, RGB_C_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_ResetBits(RGB_A_PORT, RGB_A_PIN);
    GPIO_ResetBits(RGB_B_PORT, RGB_B_PIN);
    GPIO_ResetBits(RGB_C_PORT, RGB_C_PIN);

    /* AC Contractors */
    GPIO_Init(AC_CONTRACTOR1_PORT, AC_CONTRACTOR1_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(AC_CONTRACTOR2_PORT, AC_CONTRACTOR2_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_ResetBits(AC_CONTRACTOR1_PORT, AC_CONTRACTOR1_PIN);
    GPIO_ResetBits(AC_CONTRACTOR2_PORT, AC_CONTRACTOR2_PIN);

    /* Relays */
    GPIO_Init(RELAY_C_PORT, RELAY_C_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RELAY_D_PORT, RELAY_D_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RELAY_E_PORT, RELAY_E_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RELAY_F_PORT, RELAY_F_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RELAY_G_PORT, RELAY_G_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_Init(RELAY_H_PORT, RELAY_H_PIN, GPIO_Mode_Out_PP_Low_Fast);
    GPIO_ResetBits(RELAY_C_PORT, RELAY_C_PIN);
    GPIO_ResetBits(RELAY_D_PORT, RELAY_D_PIN);
    GPIO_ResetBits(RELAY_E_PORT, RELAY_E_PIN);
    GPIO_ResetBits(RELAY_F_PORT, RELAY_F_PIN);
    GPIO_ResetBits(RELAY_G_PORT, RELAY_G_PIN);
    GPIO_ResetBits(RELAY_H_PORT, RELAY_H_PIN);

    /* Rectifier inputs */
    for (i = 0; i < 14; i++) {
        GPIO_Init(rect_ports[i], rect_pins[i], GPIO_Mode_In_FL_No_IT);
    }
}

/* ---------------- APPLY STATUS ---------------- */
void apply_output_status(void) {
    /* Relays C–H */
    (relay_status & (1<<2)) ? GPIO_SetBits(RELAY_C_PORT, RELAY_C_PIN) : GPIO_ResetBits(RELAY_C_PORT, RELAY_C_PIN);
    (relay_status & (1<<3)) ? GPIO_SetBits(RELAY_D_PORT, RELAY_D_PIN) : GPIO_ResetBits(RELAY_D_PORT, RELAY_D_PIN);
    (relay_status & (1<<4)) ? GPIO_SetBits(RELAY_E_PORT, RELAY_E_PIN) : GPIO_ResetBits(RELAY_E_PORT, RELAY_E_PIN);
    (relay_status & (1<<5)) ? GPIO_SetBits(RELAY_F_PORT, RELAY_F_PIN) : GPIO_ResetBits(RELAY_F_PORT, RELAY_F_PIN);
    (relay_status & (1<<6)) ? GPIO_SetBits(RELAY_G_PORT, RELAY_G_PIN) : GPIO_ResetBits(RELAY_G_PORT, RELAY_G_PIN);
    (relay_status & (1<<7)) ? GPIO_SetBits(RELAY_H_PORT, RELAY_H_PIN) : GPIO_ResetBits(RELAY_H_PORT, RELAY_H_PIN);

    /* RGB LEDs */
    (rgb_status & (1<<0)) ? GPIO_SetBits(RGB_A_PORT, RGB_A_PIN) : GPIO_ResetBits(RGB_A_PORT, RGB_A_PIN);
    (rgb_status & (1<<1)) ? GPIO_SetBits(RGB_B_PORT, RGB_B_PIN) : GPIO_ResetBits(RGB_B_PORT, RGB_B_PIN);
    (rgb_status & (1<<2)) ? GPIO_SetBits(RGB_C_PORT, RGB_C_PIN) : GPIO_ResetBits(RGB_C_PORT, RGB_C_PIN);

    /* AC Contractors */
    (ac_status & (1<<0)) ? GPIO_SetBits(AC_CONTRACTOR1_PORT, AC_CONTRACTOR1_PIN) : GPIO_ResetBits(AC_CONTRACTOR1_PORT, AC_CONTRACTOR1_PIN);
    (ac_status & (1<<1)) ? GPIO_SetBits(AC_CONTRACTOR2_PORT, AC_CONTRACTOR2_PIN) : GPIO_ResetBits(AC_CONTRACTOR2_PORT, AC_CONTRACTOR2_PIN);
}

/* ---------------- STRING HELPERS ---------------- */
void str_to_lower(char *str) {
    while (*str) { *str = (char)tolower((unsigned char)*str); str++; }
}

void safe_strcat(char *dest, const char *src, unsigned int max_len) {
    unsigned int dlen = strlen(dest);
    unsigned int slen = strlen(src);
    if (dlen + slen >= max_len) slen = max_len - dlen - 1;
    memcpy(dest + dlen, src, slen);
    dest[dlen + slen] = '\0';
}

char hex_digit(uint8_t n) {
    n &= 0x0F;
    return (n < 10) ? ('0' + n) : ('A' + (n - 10));
}

/* ---------------- STATUS UPDATE ---------------- */
void update_status_msg(void) {
    uint8_t i;
    memset(status_msg, 0, sizeof(status_msg));
    strcpy(status_msg, "S:0x");
    status_msg[strlen(status_msg)] = hex_digit((relay_status >> 4) & 0x0F);
    status_msg[strlen(status_msg)] = hex_digit(relay_status & 0x0F);
    status_msg[strlen(status_msg)] = '\0';

    safe_strcat(status_msg, "|", sizeof(status_msg));

    /* Relay states */
    safe_strcat(status_msg, (relay_status & (1<<2)) ? "C1 " : "C0 ", sizeof(status_msg));
    safe_strcat(status_msg, (relay_status & (1<<3)) ? "D1 " : "D0 ", sizeof(status_msg));
    safe_strcat(status_msg, (relay_status & (1<<4)) ? "E1 " : "E0 ", sizeof(status_msg));
    safe_strcat(status_msg, (relay_status & (1<<5)) ? "F1 " : "F0 ", sizeof(status_msg));
    safe_strcat(status_msg, (relay_status & (1<<6)) ? "G1 " : "G0 ", sizeof(status_msg));
    safe_strcat(status_msg, (relay_status & (1<<7)) ? "H1 " : "H0 ", sizeof(status_msg));

    /* RGB status */
    safe_strcat(status_msg, "|RGB:", sizeof(status_msg));
    for (i = 0; i < 3; i++) {
        safe_strcat(status_msg, (rgb_status & (1<<i)) ? "1" : "0", sizeof(status_msg));
    }

    /* AC Contractors */
    safe_strcat(status_msg, "|AC:", sizeof(status_msg));
    safe_strcat(status_msg, (ac_status & (1<<0)) ? "1" : "0", sizeof(status_msg));
    safe_strcat(status_msg, (ac_status & (1<<1)) ? "1" : "0", sizeof(status_msg));

    /* Rectifier inputs */
    safe_strcat(status_msg, "|R:", sizeof(status_msg));
    for (i = 0; i < 15; i++) {
        uint8_t pin_state = GPIO_ReadInputDataBit(rect_ports[i], rect_pins[i]);
        safe_strcat(status_msg, (pin_state ? "1" : "0"), sizeof(status_msg));
    }
}

/* ---------------- COMMAND PARSER ---------------- */
void relay_set(uint8_t bit, uint8_t on) {
    if (on) relay_status |= (1 << bit);
    else relay_status &= ~(1 << bit);
}

void rgb_set(uint8_t bit, uint8_t on) {
    if (on) rgb_status |= (1 << bit);
    else rgb_status &= ~(1 << bit);
}

void parse_command(const char *cmd) {
    char buf[BUFFER_SIZE];
    strncpy(buf, cmd, BUFFER_SIZE - 1);
    buf[BUFFER_SIZE - 1] = '\0';
    str_to_lower(buf);

    UART_TX_String("I2C RX: ");
    UART_TX_String(buf);
    UART_TX_String("\r\n");

    /* Relay controls */
    if (strstr(buf, "relaycon")) relay_set(2,1);
    else if (strstr(buf, "relaycoff")) relay_set(2,0);
    else if (strstr(buf, "relaydon")) relay_set(3,1);
    else if (strstr(buf, "relaydoff")) relay_set(3,0);
    else if (strstr(buf, "relayeon")) relay_set(4,1);
    else if (strstr(buf, "relayeoff")) relay_set(4,0);
    else if (strstr(buf, "relayfon")) relay_set(5,1);
    else if (strstr(buf, "relayfoff")) relay_set(5,0);
    else if (strstr(buf, "relaygon")) relay_set(6,1);
    else if (strstr(buf, "relaygoff")) relay_set(6,0);
    else if (strstr(buf, "relayhon")) relay_set(7,1);
    else if (strstr(buf, "relayhoff")) relay_set(7,0);

    /* RGB controls */
    else if (strstr(buf, "rgb1on")) rgb_set(0,1);
    else if (strstr(buf, "rgb1off")) rgb_set(0,0);
    else if (strstr(buf, "rgb2on")) rgb_set(1,1);
    else if (strstr(buf, "rgb2off")) rgb_set(1,0);
    else if (strstr(buf, "rgb3on")) rgb_set(2,1);
    else if (strstr(buf, "rgb3off")) rgb_set(2,0);

    /* AC Contractor controls */
    else if (strstr(buf, "ac1on")) ac_status |= (1<<0);
    else if (strstr(buf, "ac1off")) ac_status &= ~(1<<0);
    else if (strstr(buf, "ac2on")) ac_status |= (1<<1);
    else if (strstr(buf, "ac2off")) ac_status &= ~(1<<1);

    /* Global */
    else if (strstr(buf, "allon")) { relay_status = 0xFC; rgb_status = 0x07; ac_status = 0x03; }
    else if (strstr(buf, "alloff")) { relay_status = 0x00; rgb_status = 0x00; ac_status = 0x00; }
    else UART_TX_String("Unknown\r\n");

    apply_output_status();
    update_status_msg();
}

/* ---------------- I2C SLAVE ---------------- */
void i2c_init_slave(void) {
    CLK_PeripheralClockConfig(I2C_CLK, ENABLE);
    GPIO_Init(GPIOC, GPIO_Pin_1, GPIO_Mode_Out_OD_Low_Fast); // SCL
    GPIO_Init(GPIOC, GPIO_Pin_0, GPIO_Mode_Out_OD_Low_Fast); // SDA

    I2C_DeInit(I2C_INTERFACE);
    I2C_Init(I2C_INTERFACE, 100000, STM8_SLAVE_ADDR, I2C_Mode_I2C,
             I2C_DutyCycle_2, I2C_Ack_Enable, I2C_AcknowledgedAddress_7bit);
    I2C_Cmd(I2C_INTERFACE, ENABLE);
    I2C_AcknowledgeConfig(I2C_INTERFACE, ENABLE);
}

/* ---------------- MAIN LOOP ---------------- */
void i2c_slave_task(void) {
    uint8_t i;
    while (1) {
        while (!I2C_CheckEvent(I2C_INTERFACE, I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED));
        rx_index = 0;

        while (!I2C_CheckEvent(I2C_INTERFACE, I2C_EVENT_SLAVE_STOP_DETECTED)) {
            if (I2C_GetFlagStatus(I2C_INTERFACE, I2C_FLAG_RXNE)) {
                if (rx_index < BUFFER_SIZE - 1)
                    i2c_rx_buffer[rx_index++] = (char)I2C_ReceiveData(I2C_INTERFACE);
                else
                    (void)I2C_ReceiveData(I2C_INTERFACE);
            }
        }

        i2c_rx_buffer[rx_index] = '\0';
        I2C_ClearFlag(I2C_INTERFACE, I2C_FLAG_STOPF);
        I2C_AcknowledgeConfig(I2C_INTERFACE, ENABLE);

        if (rx_index > 0) parse_command(i2c_rx_buffer);

        while (!I2C_CheckEvent(I2C_INTERFACE, I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED));
        for (i = 0; i < 64; i++) {
            while (!I2C_GetFlagStatus(I2C_INTERFACE, I2C_FLAG_TXE));
            I2C_SendData(I2C_INTERFACE, (uint8_t)status_msg[i]);
        }
    }
}

/* ---------------- MAIN ---------------- */
int main(void) {
    CLK_SYSCLKDivConfig(CLK_SYSCLKDiv_1);
    uart_init();
    gpio_init();
    UART_TX_String("STM8 Slave Ready (Relays C–H, RGB A–C, AC1–2, 14 Rect)\r\n");
    update_status_msg();
    i2c_init_slave();
    i2c_slave_task();
    return 0;
}
