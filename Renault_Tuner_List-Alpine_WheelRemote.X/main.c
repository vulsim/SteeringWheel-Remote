/* 
 * File:   main.c
 * Author: vulsim
 *
 * Created on 28/01/2013, 21:08
 */

#include <xc.h>
#include <stdint.h>
#include <pic16f628.h>

#pragma config BOREN = OFF, CPD = OFF, FOSC = XT, MCLRE = ON, WDTE = OFF, CP = OFF, LVP = OFF, PWRTE = ON

#define _XTAL_FREQ          4000000
#define OUTPUT_MASK         0x2

////////////////////////////////////////////////////////////////////////////////

#define ALPINE_CMD_COUNT    11
#define ALPINE_PACKET_SIZE  4

const uint8_t alpine_packet[ALPINE_CMD_COUNT][ALPINE_PACKET_SIZE] = {
    {0x86, 0x72, 0x14, 0xEB},   // 0: Volume up
    {0x86, 0x72, 0x15, 0xEA},   // 1: Volume down
    {0x86, 0x72, 0x16, 0xE9},   // 2: Mute
    {0x86, 0x72, 0x0E, 0xF1},   // 3: Folder up
    {0x86, 0x72, 0x0F, 0xF0},   // 4: Folder down
    {0x86, 0x72, 0x0A, 0xF5},   // 5: Source
    {0x86, 0x72, 0x12, 0xED},   // 6: Previous track
    {0x86, 0x72, 0x13, 0xEC},   // 7: Next track
    {0x86, 0x72, 0x09, 0xF6},   // 8: Power
    {0x86, 0x72, 0x07, 0xF8},   // 9: Play
    {0x86, 0x72, 0x0D, 0xF2}    // 10: Band
};

////////////////////////////////////////////////////////////////////////////////
// Key mapping

// KEY 0 - Encoder 1
// KEY 1 - Encoder 2    { Volume up/down
// KEY 2 - Encoder 3
// KEY 3 - Band         - Band/Source
// KEY 4 - Volume+      - Previos track
// KEY 5 - Volume-      - Next track
// KEY 6 - Source-      - Folder up
// KEY 7 - Mute         - Play
// KEY 8 - Source+      - Folder down

#define KEY_COUNT           9

#define ENC_KEY_0           0
#define ENC_KEY_1           1
#define ENC_KEY_2           2

const uint8_t key_push_bit[KEY_COUNT] = {0, 4, 2, 2, 4, 0, 4, 0, 2};
const uint8_t key_pull_bit[KEY_COUNT] = {1, 1, 1, 5, 5, 5, 3, 3, 3};

const uint8_t key_mode[KEY_COUNT] = {2, 2, 2, 1, 0, 0, 0, 0, 0};
const uint8_t key_cmd1[KEY_COUNT] = {0, 0, 0, 10, 6, 7, 3, 9, 4};
const uint8_t key_cmd2[KEY_COUNT] = {1, 1, 1, 5, 6, 7, 3, 9, 4};

////////////////////////////////////////////////////////////////////////////////

void space(uint8_t ntimes)
{
    while (ntimes--) {
        __delay_us(255);
        __delay_us(255);
        __delay_us(52);
    }
}

void mark(uint8_t ntimes)
{
    PORTA |= OUTPUT_MASK;
    space(ntimes);
    PORTA ^= OUTPUT_MASK;
}

void sendCmd(uint8_t cmd)
{
    TRISA ^= OUTPUT_MASK;
    mark(16);
    space(8);

    for (int8_t nbyte = 0; nbyte < ALPINE_PACKET_SIZE; nbyte++) {
        for (int8_t nbit = 0; nbit < 8; nbit++) {
            mark(1);
            if (alpine_packet[cmd][nbyte] & (1 << nbit)) {
                space(3);
            } else {
                space(1);
            }
        }
    }
    mark(1);
    TRISA ^= OUTPUT_MASK;
}

void repeatCmd()
{
    TRISA ^= OUTPUT_MASK;
    mark(16);
    space(4);
    mark(1);
    TRISA ^= OUTPUT_MASK;
}

uint8_t keyPressed(uint8_t key)
{
    uint8_t key_push_mask = 1 << key_push_bit[key];
    uint8_t key_pull_mask = 1 << key_pull_bit[key];

    PORTB = 0x0;
    TRISB = 0xFF ^ key_push_mask;
    NOP();
    
    uint8_t result = 0xFF;

    for (uint8_t i = 0; i < 4; i++) {
        result &= PORTB & key_pull_mask;
        __delay_ms(5);
    }

    TRISB ^= key_push_mask;
    return (result) ? 1 : 0;
}

void process(uint8_t key)
{
    switch (key_mode[key]) {
        case 0:
            sendCmd(key_cmd1[key]);
            break;
        case 1:
            __delay_ms(255);
            __delay_ms(245);
            if (keyPressed(key)) {
                sendCmd(key_cmd1[key]);
            } else {
                __delay_ms(255);
                __delay_ms(245);
                
                if (keyPressed(key)) {
                    sendCmd(key_cmd2[key]);
                } else {
                    sendCmd(key_cmd1[key]);
                }
            }
            break;
        case 2: {
            static uint8_t encLastKey = 0xFF;

            if (encLastKey != 0xFF) {
                if ((encLastKey == ENC_KEY_0 && key == ENC_KEY_1) ||
                        (encLastKey == ENC_KEY_1 && key == ENC_KEY_2) ||
                        (encLastKey == ENC_KEY_2 && key == ENC_KEY_0)) {
                    sendCmd(key_cmd1[key]);
                } else if ((encLastKey == ENC_KEY_2 && key == ENC_KEY_1) ||
                        (encLastKey == ENC_KEY_1 && key == ENC_KEY_0) ||
                        (encLastKey == ENC_KEY_0 && key == ENC_KEY_2)) {
                    sendCmd(key_cmd2[key]);
                }
            }
            encLastKey = key;         
            break;
        }        
    }
    __delay_ms(41);
}

void repeat(uint8_t key)
{
    switch (key_mode[key]) {
        case 0:
            repeatCmd();
            break;
        case 1:
            repeatCmd();
            break;            
    }
    __delay_ms(41);
}

void setup() {

    INTCON = 0x0;
    TRISA = 0xFF;
    TRISB = 0xFF;
    PORTA = 0x0;
    PORTB = 0x0;
    nRBPU = 0x0;
}

void main(void) {

    static uint8_t activeKey = 0xFF;

    setup();

    while (1) {
        if (activeKey == 0xFF) {
            for (uint8_t i = 0; i < KEY_COUNT; i++) {
                if (keyPressed(i)) {
                    activeKey = i;
                    process(activeKey);
                    break;
                }
            }
            if (activeKey == 0xFF) {
                __delay_ms(10);
            }
        } else {
            if (keyPressed(activeKey)) {
                repeat(activeKey);
            } else {
                activeKey = 0xFF;
                __delay_ms(255);
                __delay_ms(245);
            }
        }
    }
}
