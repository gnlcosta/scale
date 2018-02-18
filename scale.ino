
/* 
 * Scale with Arduino Nano/Mini
 * Copyright (c) 2017 Gianluca Costa <g.costa@xplico.org>
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <HX711.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


#define ENCODER_PIN_SW        4                // Encoder variable - SW
#define ENCODER_PIN_B         2                // Encoder variable - DT
#define ENCODER_PIN_A         3                // Encoder variable - CLK
#define PUSH_MIN_TIME         100              // ms

#define OZ_TO_G                28,3495

#define E2P_VER                0x01 // 0.0
#define E2P_CHECK_PATERN       0xC057A

typedef struct {
  char ver; // x.y
  char lang; // 0: en; 1: it
  char unit; // 0: Kg, 1: lb
  unsigned long load_cel_max; // unit g or oz
  float calibr_coef; // coef to convert sensor data to unit
  unsigned long check; // always E2P_CHECK_PATERN
} setting;


// U8g2 Contructor List (Picture Loop Page Buffer)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

static HX711 scale;
static setting cfg = {// default
  .ver = E2P_VER,
  .lang = 0, // en
  .unit = 0, // kg
  .load_cel_max = 20000, 
  .calibr_coef = 101.229,
  .check = E2P_CHECK_PATERN
};

static char tara = 0;
static char calibr = 0;
static Encoder myEnc(ENCODER_PIN_A, ENCODER_PIN_B);
static volatile char push_bnt;
static const PROGMEM char main_menu_ita[] =
  "Impostazioni\n"
  "Tara\n"
  "Calibrazione\n"
  "Ritorna";
  
static const PROGMEM char setting_menu_ita[] =
  "Lingua\n"
  "Unita'\n"
  "Peso Max\n"
  "Ritorna";
  
static const PROGMEM char main_menu_en[] =
  "Settings\n"
  "Tare\n"
  "Calibration\n"
  "Go back";
  
static const PROGMEM char setting_menu_en[] =
  "Language\n"
  "Unit\n"
  "Max Weight\n"
  "Go back";
  
static const unsigned char ico_bitmap[] U8X8_PROGMEM = {
   0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x80, 0xff, 0xe3, 0xff, 0x00, 0x00,
   0x80, 0xff, 0xe3, 0xff, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x80, 0x01, 0x1c, 0xc0, 0x00, 0x00,
   0x80, 0x03, 0x1c, 0xc0, 0x00, 0x00, 0xc0, 0x02, 0x1c, 0x20, 0x01, 0x00,
   0x40, 0x06, 0x1c, 0x30, 0x03, 0x00, 0x20, 0x04, 0x1c, 0x10, 0x02, 0x00,
   0x20, 0x08, 0x1c, 0x18, 0x06, 0x00, 0x10, 0x08, 0x1c, 0x08, 0x04, 0x00,
   0x18, 0x10, 0x1c, 0x0c, 0x0c, 0x00, 0x08, 0x30, 0x1c, 0x04, 0x08, 0x00,
   0x0c, 0x20, 0x1c, 0x06, 0x18, 0x00, 0x04, 0x60, 0x1c, 0x02, 0x10, 0x00,
   0x06, 0x40, 0x1c, 0x01, 0x20, 0x00, 0x02, 0xc0, 0x1c, 0x01, 0x20, 0x00,
   0x03, 0x80, 0x9c, 0x00, 0x40, 0x00, 0xff, 0xff, 0x9c, 0xff, 0x7f, 0x00,
   0xfe, 0xff, 0x9c, 0xff, 0x7f, 0x00, 0xfe, 0x7f, 0x1c, 0xff, 0x3f, 0x00,
   0xf8, 0x3f, 0x1c, 0xfe, 0x1f, 0x00, 0xe0, 0x07, 0x1c, 0xf0, 0x03, 0x00,
   0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
   0x80, 0xff, 0xff, 0xff, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x00, 0x00
};


void MenuSettings(void)
{
  uint8_t current_selection = 0;
  const char *title, *unit;
  char save, str[40];
  unsigned int new_val;
  unsigned long max_cell;
  long enc_pos, delta;
  int len, k;
  
#if 1
  save = 0;
  do {
    if (cfg.lang == 1) { // ita
      len = strlen_P(setting_menu_ita);
      for (k = 0; k < len; k++)
        str[k] = pgm_read_byte_near(setting_menu_ita + k);
      str[k] = '\0';
      title = "IMPOSTAZIONI";
    }
    else { // en
      len = strlen_P(setting_menu_en);
      for (k = 0; k < len; k++)
        str[k] = pgm_read_byte_near(setting_menu_en + k);
      str[k] = '\0';
      title = "SETTINGS";
    }
    
    u8g2.setFont(u8g2_font_ncenB10_tr);
    current_selection = u8g2.userInterfaceSelectionList(
      title,
      current_selection, 
      str);
    if (current_selection == 1) {
      new_val = u8g2.userInterfaceMessage("", "", ""," Eng \n Ita ");
      new_val = new_val - 1;
      if (cfg.lang != new_val) {
        cfg.lang = new_val;
        save = 1;
      }
    }
    else if (current_selection == 2) {
      new_val = u8g2.userInterfaceMessage("", "", ""," kg \n lb ");
      new_val = new_val - 1;
      if (cfg.unit != new_val) {
        if (cfg.unit) {
          cfg.calibr_coef = cfg.calibr_coef / OZ_TO_G;
          cfg.load_cel_max = cfg.load_cel_max * OZ_TO_G;
        }
        else {
          cfg.calibr_coef = cfg.calibr_coef * OZ_TO_G;
          cfg.load_cel_max = cfg.load_cel_max / OZ_TO_G;
        }
        scale.set_scale(cfg.calibr_coef);
        cfg.unit = new_val;
        save = 1;
      }
    }
    else if (current_selection == 3) {
      while (push_bnt == 0)
        ;
      if (cfg.lang == 1) { // ita
        title = "Cella di carico";
      }
      else { // en
        title = "Load Cell";
      }
      if (cfg.unit == 1) {
        unit = " oz";
      }
      else {
        unit = " g";
      }
      max_cell = cfg.load_cel_max;
      enc_pos = myEnc.read();
      do {
        u8g2.firstPage();
        do {
          u8g2.setFont(u8g2_font_ncenB12_tr);
          u8g2.setCursor((128-u8g2.getStrWidth(title))/2, 12);
          u8g2.print(title);
          u8g2.setCursor((128-u8g2.getStrWidth("MAX"))/2, 28);
          u8g2.print("MAX");
          u8g2.setFont(u8g2_font_ncenB14_tr);
          sprintf(str, "%lu %s", max_cell, unit);
          u8g2.setCursor((128-u8g2.getStrWidth(str))/2, 54);
          u8g2.print(str);
        } while ( u8g2.nextPage() );
        // upgrade
        delta = (myEnc.read() - enc_pos)/2;
        if (delta) {
          max_cell += delta;
          enc_pos = myEnc.read();
        }
      } while (push_bnt);
      while (push_bnt == 0)
        ;
      cfg.load_cel_max = max_cell;
      save = 1;
    }
  } while (current_selection != 4);
  
  if (save)
    EEPROM.put(0, cfg);
#endif
}


static void MenuMain(void)
{
  uint8_t current_selection = 1;
  char string_list[50];
  int len, k;
  
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    if (cfg.lang == 1) {
      len = strlen_P(main_menu_ita);
      for (k = 0; k < len; k++)
        string_list[k] = pgm_read_byte_near(main_menu_ita + k);
      string_list[k] = '\0';
    }
    else {
      len = strlen_P(main_menu_en);
      for (k = 0; k < len; k++)
        string_list[k] = pgm_read_byte_near(main_menu_en + k);
      string_list[k] = '\0';
    }
    
    current_selection = u8g2.userInterfaceSelectionList(
      "MENU",
      current_selection, 
      string_list);
  
    switch (current_selection) {
    case 1:
      MenuSettings();
      break;
      
    case 2:
      tara = 1;
      break;
      
    case 3:
      calibr = 1;
      break;
    }
  } while (current_selection == 1);
}
  
uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
  static char push = 0;
  static long enc_pos = -9999;
  long enc_rt;

  enc_rt = (myEnc.read()/2);
  if (enc_pos == -9999)
    enc_pos = enc_rt;
  
  if (push == 1 && push_bnt == 0) {
    enc_pos = -9999;
    push = push_bnt;
    return U8X8_MSG_GPIO_MENU_SELECT;
  }
  push = push_bnt;
    
  if (enc_pos < enc_rt) {
    enc_pos = enc_rt;
    return U8X8_MSG_GPIO_MENU_NEXT;
  }
  else if (enc_pos > enc_rt) {
    enc_pos = enc_rt;
    return U8X8_MSG_GPIO_MENU_PREV;
  }
  
  return 0;
}


static void Tara(void)
{
  unsigned short go = 128;
  const char *title;

#if 0
  if (cfg.lang == 1)
    title = "TARA";
  else
    title = "TARE";
  
  do {
    u8g2.setFont(u8g2_font_ncenB12_tr);
    u8g2.firstPage();
    do {
      u8g2.setCursor((128-u8g2.getStrWidth(title))/2, 24);
      u8g2.print(title);
      u8g2.drawDisc(go, 55, 4+(go%5));
    } while ( u8g2.nextPage() );
    if ((go%64) == 0)
      scale.tare();
    go--;
  } while (go);
#else
scale.tare();
#endif
}


static void Weight(void)
{
  static float peso_raw = 0;
  float peso;
  char p[30], str[30];
  char *unit_msr;
  short virgola = 1;
  unsigned short level;

  peso_raw = peso_raw*0.3 + scale.get_units(5)*0.7;
  if (peso_raw < 0) {
    peso_raw = 0;
  }
  level = peso_raw/cfg.load_cel_max*118;
  if (cfg.unit == 0) {
    if (peso_raw > 1000) {
       peso = peso_raw / 1000;
       unit_msr = "kg";
       virgola = 2;
    }
    else if (peso_raw > 100) {
      peso = peso_raw / 100;
      unit_msr = "hg";
      virgola = 2;
    }
    else {
      peso = peso_raw;
      unit_msr = "g";
    }
  }
  else {
    if (peso_raw > 16) {
      peso = peso_raw / 16;
      unit_msr = "lb";
      virgola = 2;
    }
    else {
      peso = peso_raw;
      unit_msr = "oz";
    }
  }
  dtostrf(peso, 4, virgola, p);
  sprintf(str, "%s %s", p, unit_msr);
  u8g2.firstPage();
  do {
    // weight
    u8g2.setFont(u8g2_font_ncenB24_tr);
    u8g2.setCursor((128-u8g2.getStrWidth(str))/2, 24);
    u8g2.print(str);
    u8g2.drawFrame(5, 34, 118, 3);
    u8g2.drawBox(5, 34, level, 3);
    u8g2.drawDisc(level+5, 35, 3);
    
    // statistics data
    
  } while (u8g2.nextPage());
}


static void Calibration()
{
  const char *title, *title1, *title2, *unit;
  char save, str[20], value[10];
  unsigned int new_val;
  float weight, sensor;
  long enc_pos, delta;
  
#if 1
  if (cfg.lang == 1) {
    title = "Rimuovere";
    title1 = "ogni";
    title2 = "oggetto";
  }
  else {
    title = "Remove";
    title1 = "any";
    title2 = "object";
  }
  new_val = u8g2.userInterfaceMessage(title, title1, title2," Ok ");
  scale.set_scale();
  scale.tare();
  
  if (cfg.lang == 1) {
    title = "Poggiare";
    title1 = "il peso";
    title2 = "campione";
  }
  else {
    title = "Put";
    title1 = "the sample";
    title2 = "weight";
  }
  new_val = u8g2.userInterfaceMessage(title, title1, title2," Ok ");
  
  if (cfg.lang == 1) {
    title = "Peso Campione";
  }
  else {
    title = "Sample Weight";
  }
  if (cfg.unit == 1) {
    unit = " oz";
  }
  else {
    unit = " g";
  }
  sensor = scale.get_units(10);
  weight = 1000.0;
  enc_pos = myEnc.read();
  do {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_ncenB12_tr);
      u8g2.setCursor((128-u8g2.getStrWidth(title))/2, 12);
      u8g2.print(title);
      u8g2.setFont(u8g2_font_ncenB14_tr);
      dtostrf(weight, 4, 1, value);
      sprintf(str, "%s %s", value, unit);
      u8g2.setCursor((128-u8g2.getStrWidth(str))/2, 44);
      u8g2.print(str);
    } while ( u8g2.nextPage() );
    // upgrade
    delta = (myEnc.read() - enc_pos)/2;
    if (delta) {
      weight += ((float)delta)/10;
      enc_pos = myEnc.read();
    }
  } while (push_bnt);
  while (push_bnt == 0)
    ;

  // new coef
  cfg.calibr_coef = sensor / weight;
  scale.set_scale(cfg.calibr_coef);

  // save
  EEPROM.put(0, cfg);
#endif
}

static void LoadSettings(void)
{
  // check data
  setting cfg_load;

  EEPROM.get(0, cfg_load);
  
  if (cfg_load.check == E2P_CHECK_PATERN && cfg.ver == E2P_VER) {
    cfg = cfg_load;
  }
  else {
    EEPROM.put(0, cfg);
  }
}


ISR(TIMER2_COMPA_vect)
{
  static char push = 0;
  static unsigned short to_push = PUSH_MIN_TIME;

  if (push == digitalRead(ENCODER_PIN_SW)) {
    if (to_push == 0) {
      push_bnt = push;
    }
    else
      to_push--;
  }
  else {
    push = digitalRead(ENCODER_PIN_SW);
    to_push = PUSH_MIN_TIME;
  }
}


void setup(void) {
  const char *title;
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  // initialize timer
  noInterrupts();           // disable all interrupts
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register for 8khz increments
  OCR2A = 248;// = (16*10^6) / (1000*64) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 and CS20 bits for 64 prescaler
  TCCR2B |= (1 << CS21) | (1 << CS20);   
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  interrupts();             // enable all interrupts

  // settings
  LoadSettings();
  
  if (cfg.lang == 1)
    title = "Bilancia";
  else
    title = "Balance";

  // display
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB12_tr);
  // logo
  u8g2.firstPage();
  do {
    u8g2.setCursor((128-u8g2.getStrWidth(title))/2, 24);
    u8g2.print(title);
    u8g2.drawXBMP((128-41)/2, 32, 41, 32, (unsigned char*)ico_bitmap);
  } while ( u8g2.nextPage() );

  // push button
  pinMode(ENCODER_PIN_SW, INPUT_PULLUP);

  // scale
  scale.begin(A1, A0);
  scale.set_scale(cfg.calibr_coef);   // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();               // reset the scale to 0
  delay(2000);
  scale.tare();               // reset the scale to 0
}


void loop(void)
{
  // encoder
  if (push_bnt == 0) {
    MenuMain();
  }
  
  if (tara) {
    tara = 0;
    Tara();
  }
  else if (calibr) {
    calibr = 0;
    Calibration();
  }
  else {
    Weight();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

