// uWatch
// Tom Jacobs

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/util/datetime.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/types.h"
#include "pico/bootrom/sf_table.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "hardware/gpio.h"
#include <hardware/flash.h>
#include "hardware/watchdog.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include "lcd.h"
#include "lib/draw.h"

#include "QMI8658.h"

#include "img/Font34.h"
#include "img/Font30.h"
#include "img/bega.h"
#include "img/sand.h"
#include "img/col1.h"
#include "img/irisa190.h"
#include "img/earth190.h"
#include "img/usa32.h"
#include "img/cn32.h"
#include "img/ger32.h"
#include "img/tr32.h"
#include "img/usa16.h"
#include "img/cn16.h"
#include "img/ger16.h"
#include "img/tr16.h"
#include "img/flag_ch16.h"
#include "img/flag_ch32.h"
#include "img/flag_gb16.h"
#include "img/flag_gb32.h"
#include "img/config.h"
#include "img/conf_exit.h"
#include "img/conf_background.h"
#include "img/conf_handstyle.h"
#include "img/conf_rotozoom.h"
#include "img/conf_rotate.h"
#include "img/conf_save.h"
#include "img/conf_clock.h"
#include "img/conf_bender.h"
#include "img/w2.h"
#include "img/tiles_blue.h"
#include "img/flow.h"
#include "img/l3.h"
#include "img/gt.h"

// Graphics mode
typedef enum {
    GFX_NORMAL,
    GFX_ROTOZOOM,
    GFX_ROTATE,
} GFX_MODE;

// Battery
typedef struct Battery_t {
    char mode[8];
    float mA;
    float load;
    float max;
    float min;
    float diff;
    float read;
} Battery_t;

// Storage
typedef struct {
    char mode[8];
    datetime_t dt;
    Battery_t bat;
    uint8_t theme;
    uint8_t editpos;
    uint8_t BRIGHTNESS;
    bool sensors;
    bool gyrocross;
    bool bender;
    bool SMOOTH_BACKGROUND;
    bool INSOMNIA;
    bool DYNAMIC_CIRCLES;
    bool DEEPSLEEP;
    bool is_sleeping;
    bool highpointer;
    bool alphapointer;
    bool clock;
    bool pointerdemo;
    bool rotoz;
    bool rota;
    GFX_MODE gfxmode;
    PSTYLE pstyle;
    int16_t spin;
    uint8_t texture;
    uint16_t configpos;
    uint8_t conf_bg;
    uint8_t conf_phour;
    uint8_t conf_pmin;
    uint8_t scandir;
    bool dither;
    uint8_t dummy;
    uint8_t save_crc;
} LOSA_t;
static __attribute__((section (".noinit")))char storage[4096];
static LOSA_t* plosa=(LOSA_t*)storage;
#define LOSASIZE (&plosa->dummy - &plosa->theme)

// Functions
uint8_t crc(uint8_t *addr, uint32_t len);
void sincosf(float, float*, float*);
void dosave();

// Start date time
datetime_t default_time = {
    .year  = 2023,
    .month = 1,
    .day   = 16,
    .dotw  = 1, // 0 is Sunday
    .hour  = 12,
    .min   = 0,
    .sec   = 0
};

// Spin degrees
int16_t flagdeg = 90;

int16_t flagdeg1 = 30;
int16_t flagdeg2 = 60;
int16_t flagdeg1a = 30;
int16_t flagdeg2a = 60;
int16_t flagdeg1b = 130;
int16_t flagdeg2b = 260;

int16_t fdegs[7] = {90, 30, 60, 30, 60, 130, 260};
int16_t edeg_fine=270;
int16_t gdeg_fine=0;
int16_t b2s=75;

// Theme
#define DEFAULT_THEME 1
#define NO_POS_MODE 1
#define SHELL_ENABLED 1

// Sleep
#define SLEEP_FRAME_START 1000
#define SLEEP_FRAME_END   30000
#define SLEEP_FRAME_ADD   100

// Battery
Battery_t bat0 = {"GOOD\0", 150.0f, 4.16f, 3.32f, 4.15f, 0.0f, 0.0f}; // 150mA
Battery_t* bat_default= &bat0;
const float conversion_factor = 3.3f / (1 << 12) * 2;
#define BAT_NUMRES 16
uint16_t resulti = BAT_NUMRES - 1;
float result[BAT_NUMRES] = {0.0f};
float resultsum() {
    float sum = 0.0f;
    for(uint8_t i = 0; i < BAT_NUMRES; i++) {
        sum += result[i];
    }
    return sum;
}
float resultsummid() {
    float sum = 0.0f;
    for(uint8_t i = 0; i < BAT_NUMRES; i++) {
        sum += result[i];
    }
    return sum / BAT_NUMRES;
}
float resultminmaxmid() {
    float min = 99.0f, max = 0.0f;
    for (uint8_t i = 0; i < BAT_NUMRES; i++) {
        if (result[i] < min) min = result[i];
        if (result[i]>max) max = result[i];
    }
    return (min + max) / 2.0f;
}
float read_battery() {
    plosa->bat.read = adc_read() * conversion_factor;
    if (plosa->bat.read < plosa->bat.load) {
        //printf("battery_read: %f\n",plosa->bat.read);
        if (plosa->bat.read > plosa->bat.max) plosa->bat.max = plosa->bat.read;
        if (plosa->bat.read < plosa->bat.min) {
            plosa->bat.min = plosa->bat.read;

            // Save if battery drops down
            if (plosa->is_sleeping && plosa->BRIGHTNESS == 0) dosave();
        }

        // Battery change
        if (plosa->bat.diff != plosa->bat.max - plosa->bat.min) {
            printf("battery_diff: %f\n",plosa->bat.diff);
        }
        plosa->bat.diff = plosa->bat.max - plosa->bat.min;
    }
    return plosa->bat.read;
}

// Memcopy
#define mcpy(d, s, sz) for(int i = 0; i < sz; i++) { d[i] = s[i];}

// Fonts
#define TFONT Font20
#define CNFONT Font30

#define EYE irisa190

#define FRAME_DELAY 50
#define LOOPWAIT 50

#define DRAW_GFX_FIRST true //1 == text floating above clock
#define HOURGLASSBORDER 200 // minimum rise/fall of acc_x
#define HOURGLASS 1000*(100/LOOPWAIT)  // rise/fall of acc_x border till switch (cw/ccw)
#define BUTTONGLASSC 300
#define BUTTONGLASS 1400
#define SCRSAV 15*(100/LOOPWAIT)
#define SCRSAV2 SCRSAV*2
#define BRIGHTD 20
#define SWITCH_THEME_DELAY 10
#define THRS 12
#define THRLY 120

// Positions
#define POS_CX 120
#define POS_CY 120

#define POS_ACC_X 60
#define POS_ACC_Y 40

#define POS_BAT_X 70
#define POS_BAT_Y 30
#define POS_BAT_YS 10
#define POS_BAT_PS 2

#define POS_DATE_X 46
#define POS_DATE_Y 66

#define POS_DOW_X 20
#define POS_DOW_Y 111
#define POS_CNDOW_X 190
#define POS_CNDOW_Y 72

#define POS_TIME_X 64
#define POS_TIME_Y 156

// X column widths
#define TFW 14

// Eye dimensions
#define EYE_SZ 190
#define EYE_R EYE_SZ/2
#define EYE_X 120-EYE_R
#define EYE_Y 120-EYE_R
#define EYE_MAX 25-1

typedef enum CMode {
    CM_None = 0,
    CM_Config = 1,
    CM_Editpos = 2,
    CM_Changepos = 3,
    CM_Changetheme = 4
} CMode;

typedef struct ColorTheme_t {
    uint16_t alpha;
    uint16_t col_h;
    uint16_t col_m;
    uint16_t col_s;
    uint16_t col_cs;
    uint16_t col_cs5;
    uint16_t col_dotw;
    uint16_t col_date;
    uint16_t col_time;
    uint16_t bat_level;
    uint16_t bat_level_low;
    uint16_t bat_level_critical;
} ColorTheme_t;

typedef struct PXY_t {
    uint8_t x;
    uint8_t y;
} PXY_t;

typedef struct ThemePos_t {
    PXY_t pos_dow;
    PXY_t pos_day;
    PXY_t pos_month;
    PXY_t pos_year;
    PXY_t pos_h;
    PXY_t pos_m;
    PXY_t pos_s;
} ThemePos_t;

#define EDITPOSITIONS 8
PXY_t tpos[EDITPOSITIONS+1] =
{   POS_DOW_X,POS_DOW_Y,
    POS_DATE_X,POS_DATE_Y,
    POS_DATE_X+3*TFW,POS_DATE_Y,
    POS_DATE_X+8*TFW,POS_DATE_Y,
    POS_TIME_X,      POS_TIME_Y,
    POS_TIME_X+3*TFW,POS_TIME_Y,
    POS_TIME_X+6*TFW,POS_TIME_Y,
    POS_CX-100,POS_CY,
    POS_CX, POS_CY
};

#define EPOS_CENTER EDITPOSITIONS
#define EPOS_CONFIG EDITPOSITIONS-1

uint8_t pos_matrix_x=1; // start in center
uint8_t pos_matrix_y=1; // start in center

/* Position Matrices
it's like a gear shifter
[ | / \] : up/down movement ways
[ -    ] : left/right movement ways

example us [3*3]:
DAY-MON-YEAR
 |   |  /
DOW-FLAG
 |   |  \
H  - M - S

*/

// pos points to position matrix, containing possible 'editpos' (positions)
typedef struct PosMat_t {
    uint8_t dim_x;
    uint8_t dim_y;
    uint8_t* pos;
} PosMat_t;

int8_t pos_matrix_CN[] =
{
    1,2,3,0,
    7,8,8,0,
    4,5,6,0
};

int8_t pos_matrix_US[] =
{
    1,2,3,
    0,8,7,
    4,5,6
};

PosMat_t p_us = {3,3,pos_matrix_US};
PosMat_t p_cn = {4,3,pos_matrix_CN};

#define USA_Old_Glory_Red  0xDDDD
#define USA_Old_Glory_Blue 0xDDDD

#define CN_Red 0xE8E4
#define CN_Gold 0xFFE0

#define GER_Gold 0xFE60
#define GER_Red 0xF800

#define TR_Red 0xF800
#define TR_White 0xFFFF

#define GB_Blue 0x010D
#define GB_Red  0xC885
#define GB_White 0xFFFF

#define CH_Red 0xD943
#define CH_White 0xFFFF

#define THEMES 6

CMode cmode = CM_None;
int16_t xold, xoldt;
int16_t yold, yoldt;

const PosMat_t* positions[THEMES] = {&p_cn,&p_us,&p_us,&p_us,&p_us,&p_us};
const uint8_t* flags[THEMES] = {cn32,usa32,ger32,tr32,flag_gb32,flag_ch32};
const uint8_t* stars[THEMES] = {cn16,usa16,ger16,tr16,flag_gb16,flag_ch16};

#define MAX_BG 5
#define TEXTURES 5
uint16_t pd_tex = 0;
const char* textures[TEXTURES] = {w2,flow,tiles_blue,l3,gt};
Vec2 texsize[TEXTURES] = {128,20, 128,20, 128,19, 128,25, 120,26  };
Vec2 psize_h[TEXTURES] = {75,20,   75,20,  75,19,  80,25,  75,20  };
Vec2 psize_m[TEXTURES] = {102,16,  102,10, 102,10, 118,25, 102,10 };
const char* backgrounds[MAX_BG] = {earth190,irisa190,bega,col1,sand};
const int16_t bg_size[MAX_BG] = {190,190,240,240,240};

const bool bg_dynamic[MAX_BG] = {true,true,false,false,false};

const uint16_t edit_colors[THEMES] = {ORANGE,YELLOW,ORANGE,ORANGE,ORANGE,ORANGE};
const uint16_t change_colors[THEMES] = {YELLOW,YELLOW,YELLOW,YELLOW,YELLOW,YELLOW};

uint8_t theme_bg_dynamic_mode = 0;

ColorTheme_t colt2= {BLACK,USA_Old_Glory_Red,USA_Old_Glory_Blue,NWHITE,USA_Old_Glory_Red,WHITE,WHITE,WHITE,WHITE,WHITE,YELLOW,RED};

ColorTheme_t* colt[THEMES];

#define MAX_CONF 8
#define MAX_CONFD (360/MAX_CONF)
#define MAX_FCONF 4
#define MAX_FCONFD (360/MAX_CONF)

typedef enum {
    CP_EXIT=0,
    CP_BACKGROUND,
    CP_ROTOZOOM,
    CP_ROTATION,
    CP_SAVE,
    CP_PENSTYLE,
    CP_WAND,
    CP_PENCIL=7,
} CONF_POS;

const uint8_t* config_images[MAX_CONF+1] = {conf_exit,conf_background,conf_rotozoom,conf_rotate,conf_save,conf_handstyle,conf_clock,conf_bender};

void update_pos_matrix() {
    if(pos_matrix_x>=positions[plosa->theme]->dim_x) {
        pos_matrix_x=positions[plosa->theme]->dim_x-1;
    }
    if(pos_matrix_y>=positions[plosa->theme]->dim_y) {
        pos_matrix_y=positions[plosa->theme]->dim_y-1;
    }
}

void repos(uint8_t id) {
    for(uint8_t j=0; j<positions[plosa->theme]->dim_y; j++) {
        for(uint8_t i=0; i<positions[plosa->theme]->dim_x; i++) {
            if(id == positions[plosa->theme]->pos[j*(positions[plosa->theme]->dim_x)+i]) {
                pos_matrix_x = i;
                pos_matrix_y = j;
                break;
            }
        }
    }
}

extern Vec2 vO;
extern Vec2 v32;

uint16_t dcol = WHITE;
uint16_t editcol = YELLOW;
uint16_t changecol = YELLOW;
uint16_t acol=WHITE;
uint16_t colors[EDITPOSITIONS+1]  = {WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE};
uint16_t dcolors[EDITPOSITIONS+1] = {WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE};

uint16_t blinker[2] = {BLUE,RED};

typedef enum Dir_t {
    D_NONE = 0,
    D_PLUS = 1,
    D_MINUS = 2
} Dir_t;

Dir_t dir_x;
Dir_t dir_y;
uint8_t no_pos_x=0;
uint8_t no_pos_y=0;

int16_t gyrox=0;
int16_t gyroy=0;

uint32_t sleep_frame = SLEEP_FRAME_START;

char timebuffer[16] = {0};
char* ptimebuffer=timebuffer;
bool h24=true;

uint16_t comi=0;
char combufa[256]= {0};
int16_t comt;
uint8_t comc;

uint8_t* b0=NULL;

//shell vars!
int16_t bcx0 = 80;
int16_t bcy0 = 80;
int16_t bcx1 = 80;
int16_t bcy1 = 80;

int16_t tpoy = 30;
int16_t tpox = 22;

int16_t tpol = -22;
int16_t tpor = 22;

//ky-040
#define CCLK 16
#define CDT 17
#define CSW 19

//one button /
#define CBUT0 22
#define CBUT1 3
uint32_t nopvar;
bool fire_pressed=false;
bool analog_seconds=false;
uint32_t fire_counter=0;
bool fire=false;
bool ceasefire=false;
bool tcw=false;
bool tccw=false;
bool clk,dt,sw,oclk,odt,osw;
bool temp_read=false;
int gc=0;
char gch;
char gbuf[2] = {'c','d'};
uint32_t last_wait;
uint32_t stime;
uint8_t tseco;
int16_t hourglass_x=HOURGLASS;
int16_t hourglass_y=HOURGLASS;

bool hg_enabled=false;
int16_t hgx=0;
int16_t hgy=0;

int16_t buttonglass=BUTTONGLASS;
int16_t screensaver=SCRSAV;

int flagsdelay = SWITCH_THEME_DELAY;
int blink_counter = 0;
bool bmode = false;

extern float tsin[DEGS];
extern float tcos[DEGS];

bool edittime=false;
bool changetime=false;
char dbuf[8];
float temperature = -99.99f;

float mag[3];
bool usb_loading = false;
bool draw_gfx_first = DRAW_GFX_FIRST;
bool draw_text_enabled = true;
bool draw_gfx_enabled = true;
bool draw_config_enabled = false;
bool draw_flagconfig_enabled = false;

// config symbol
DOImage* doi_config;
DOImage* doi_config_cn;

DOImage** adoi_config[THEMES] = {&doi_config_cn,&doi_config,&doi_config,&doi_config,&doi_config,&doi_config};

float acc[3], gyro[3];
unsigned int tim_count = 0;
float last_z = 0.0f;

uint16_t cn_chars=0;
char ftst[128*4] = {0};

char* week_usa[7] = {"Sun\0","Mon\0","Tue\0","Wed\0","Thu\0","Fri\0","Sat\0"};
char* week_gb[7] = {"Sun\0","Mon\0","Tue\0","Wed\0","Thu\0","Fri\0","Sat\0"};
char* week_cn[7] = {"星期日\0","星期一\0","星期二\0","星期三\0","星期四\0","星期五\0","星期六\0"};
char* week_ger[7] = {"Son\0","Mon\0","Die\0","Mit\0","Don\0","Fre\0","Sam\0"};
char* week_ch[7] = {"Son\0","Mon\0","Die\0","Mit\0","Don\0","Fre\0","Sam\0"};
char* week_tr[7] = {"PAZ\0","PZT\0","SAL\0","CAR\0","PER\0","CUM\0","CMT\0"};
char** week[THEMES] = {week_cn,week_usa,week_ger,week_tr,week_gb,week_ch};
uint8_t last[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

char cn_buffer[32] = {0};

bool do_reset = true;
bool force_no_load = true;
bool is_flashed = false;
char crcstatus[32] = {"\0"};
char flashstatus[32] = {"\0"};

void __no_inline_not_in_flash_func(flash_data_load)() {
    uint32_t xip_offset = 0xb0000;
    char *p = (char *)XIP_BASE+xip_offset;
    for(size_t i=0; i<FLASH_SECTOR_SIZE; i++) {
        storage[i]=p[i];
    }
}

void __no_inline_not_in_flash_func(flash_data)() {
    printf("FLASHING SAVE (c%d)\n",get_core_num());
    uint32_t xip_offset = 0xb0000;
    char *p = (char *)XIP_BASE+xip_offset;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase (xip_offset, FLASH_SECTOR_SIZE);
    flash_range_program (xip_offset, (uint8_t*)storage, FLASH_SECTOR_SIZE);
    for(size_t i=0; i<FLASH_SECTOR_SIZE; i++) {
        storage[i]=p[i];
    }
    restore_interrupts(ints);
    printf("FLASHED!\n");
}

void check_save_data() {
    uint8_t acrc = crc(&plosa->theme, LOSASIZE);
    bool crc_status = (acrc==plosa->save_crc);
    sprintf(crcstatus,"CRC: [%02x][%02x] %s\n\0",acrc,plosa->save_crc,(crc_status)?"OK":"ERR");
    if (strstr((char*)plosa->bat.mode,"GOOD") != plosa->bat.mode) {
        plosa->bat.mA = bat_default->mA;
        plosa->bat.load = bat_default->load;
        plosa->bat.max = bat_default->max;
        plosa->bat.min = bat_default->min;
        plosa->bat.diff = bat_default->diff;
        plosa->bat.read = bat_default->read;
        sprintf(plosa->bat.mode,"GOOD\0");
        printf("bat mode reset to defaults='%s'\n",plosa->bat.mode);
    }
    if (strstr((char*)plosa->mode,"LOAD") != plosa->mode) {
        plosa->dt.year  = default_time.year ;
        plosa->dt.month = default_time.month;
        plosa->dt.day   = default_time.day  ;
        plosa->dt.dotw  = default_time.dotw ;
        plosa->dt.hour  = default_time.hour ;
        plosa->dt.min   = default_time.min  ;
        plosa->dt.sec   = default_time.sec  ;
        plosa->theme = DEFAULT_THEME;
        plosa->editpos = EDITPOSITIONS;
        plosa->is_sleeping = false;
        plosa->highpointer = false;
        plosa->alphapointer = true;
        plosa->pointerdemo = false;
        plosa->pstyle = PS_ALPHA;
        plosa->clock = true;
        plosa->spin = 0;
        plosa->texture = 0;
        plosa->configpos = 0;
        plosa->conf_bg = 0;
        plosa->conf_pmin = 0;
        plosa->conf_phour = 0;
        plosa->rota = false;
        plosa->rotoz = false;
        plosa->gfxmode = GFX_NORMAL;
        plosa->sensors = false;
        plosa->gyrocross = true;
        plosa->DYNAMIC_CIRCLES = false;
        sprintf(plosa->mode,"LOAD\0");
        printf("settings reset to defaults");
    } else {
        plosa->dt.sec+=1;
        if(plosa->dt.month > 12 ) {
            plosa->dt.month = default_time.month;
        }
        if(plosa->dt.day   > 31) {
            plosa->dt.day   = default_time.day  ;
        }
        if(plosa->dt.dotw  > 6)  {
            plosa->dt.dotw  = default_time.dotw ;
        }
        if(plosa->dt.hour  > 23) {
            plosa->dt.hour  = default_time.hour ;
        }
        if(plosa->dt.min   > 59) {
            plosa->dt.min   = default_time.min  ;
        }
        if(plosa->dt.sec   > 59) {
            plosa->dt.sec   = default_time.sec  ;
        }
        if(plosa->theme>=THEMES) {
            plosa->theme=0;
        }
        if(plosa->editpos>EDITPOSITIONS) {
            plosa->editpos=EDITPOSITIONS;
        }
        if(plosa->conf_bg>=MAX_BG) {
            plosa->conf_bg=0;
        }
        if(plosa->gfxmode>GFX_ROTATE) {
            plosa->gfxmode=GFX_NORMAL;
        }
        if(plosa->spin>5) {
            plosa->spin=5;
        }
        if(plosa->texture>=TEXTURES) {
            plosa->texture=0;
        }
        if(plosa->configpos>=MAX_CONF) {
            plosa->configpos = 0;
        }
        if(plosa->pstyle>=PS_TEXTURE) {
            plosa->pstyle = PS_TEXTURE;
        }
        //plosa->pointerdemo = false;
        //plosa->pstyle = PS_NORMAL;
        //plosa->clock = true;
        //plosa->spin = 0;
        //plosa->texture = 0;
        //plosa->configpos = 0;
        //plosa->conf_pmin = 0;
        //plosa->conf_phour = 0;
        //plosa->rota = false;
        //plosa->rotoz = false;
        //rtc_set_datetime(&plosa->dt);
        printf("MODE:='%s'\n",plosa->mode);
    }
}

uint8_t crc(uint8_t *addr, uint32_t len) {
    uint8_t crc = 0;
    while (len != 0) {
        uint8_t i;
        uint8_t in_byte = *addr++;
        for (i = 8; i != 0; i--) {
            uint8_t carry = (crc ^ in_byte ) & 0x80;        /* set carry */
            crc <<= 1;                                      /* left shift 1 */
            if (carry != 0) {
                crc ^= 0x7;
            }
            in_byte <<= 1;                                  /* left shift 1 */
        }
        len--;                                              /* len-- */
    }
    return crc;                                               /* return crc */
}

void empty_deinit() {
    plosa->save_crc = crc(&plosa->theme,LOSASIZE);
}

void doreset() {
    watchdog_reboot((uint32_t)&empty_deinit,0,1);
    watchdog_enable(1, 1);
}

void dosave() {
    sprintf((char*)&plosa->mode,"SAVE");
    doreset();
}

void draw_pointer(Vec2 vs, Vec2 vts, int16_t tu, uint16_t color, const uint8_t* sr, uint16_t alpha) {
    draw_pointer_mode(vs,vts,tu,color,sr,alpha,plosa->pstyle);
}

uint16_t to_rgb565(uint8_t r,uint8_t g,uint8_t b) {
    r>>=3;
    g>>=2;
    b>>=3;
    return ((r<<11)+(g<<5)+b);
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r<<11)+(g<<5)+b);
}

void to_rgb(uint16_t rgb, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r=((rgb>>11)&0x1f)<<3;
    *g=((rgb>>5)&0x3f)<<3;
    *b=(rgb&0x1f)<<3;
}

void set_dcolors() {
    for(int i=0; i<7; i++) {
        colors[i]=dcolors[i];
    }
}

void set_colt_colors() {
    dcolors[0]=colt[plosa->theme]->col_dotw;
    dcolors[1]=colt[plosa->theme]->col_date;
    dcolors[2]=colt[plosa->theme]->col_date;
    dcolors[3]=colt[plosa->theme]->col_date;
    dcolors[4]=colt[plosa->theme]->col_time;
    dcolors[5]=colt[plosa->theme]->col_time;
    dcolors[6]=colt[plosa->theme]->col_time;
    dcolors[7]=colt[plosa->theme]->col_time;
}

uint8_t find_cc(uint8_t a, uint8_t b, uint8_t c) {
    uint fo=0;
    for(int i=0; i<cn_chars+1; i++) {
        //printf("[%02x%02x%02x] %02x %02x %02x\n",a,b,c,ftst[fo],ftst[fo+1],ftst[fo+2]);
        if( (ftst[fo+0]==a) && (ftst[fo+1]==b) && (ftst[fo+2]==c) ) {
            //printf("find_cc: %d %d\n",i,i+228);
            return i;
        }
        fo+=4;
    }
}

void convert_cs(char* source, char* target) {
    uint32_t si=0, ti=0;
    while(source[si]) {
        //printf("%02x %02x %02x\n",source[si],source[si+1],source[si+2]);
        target[ti]=find_cc(source[si],source[si+1],source[si+2]);
        //printf("%d [%d]\n",target[ti],target[ti]+228);
        si+=3;
        target[ti]+=(256-32);
        ++ti;
        target[ti]+='\n';
    }
    target[ti]=0;
}

void print_font_table() {
    uint8_t fts=0;
    uint8_t n=0;
    uint8_t nbytes=0;
    uint32_t ft[128];
    uint32_t sti=0;
    char ftc[5] = {0};
    uint8_t cbu[5];
    printf("TESTING...");
    printf("symcheck\n");
    char* pc;
    for(int i=0; i<7; i++) {
        int c=0;
        pc = week_cn[i];
        while(pc[c]) {
            n=pc[c];

            if((0b10000000&n)==0b00000000) {
                nbytes=1;
            }
            if((0b11100000&n)==0b11000000) {
                nbytes=2;
            }
            if((0b11110000&n)==0b11100000) {
                nbytes=3;
            }
            if((0b11111000&n)==0b11110000) {
                nbytes=4;
            }
            //printf("n=%02x (%02b) [%d]",n,(n&0b10000000),nbytes);
            switch(nbytes) {
            case 1:
                ft[fts]=n;
                c+=1;
                break;
            case 2:
                ft[fts]=(pc[c+1]<<8)+(pc[c+0]);
                c+=2;
                break;
            case 3:
                ft[fts]=(pc[c+0]<<16)+(pc[c+1]<<8) +(pc[c+2]);
                c+=3;
                break;
            case 4:
                ft[fts]=(pc[c+0]<<24)+(pc[c+1]<<16)+(pc[c+2]<<8)+(pc[c+3]);
                c+=4;
            }
            //printf("ft=%d\n",ft[fts]);
            bool dupe=false;
            for(int j=0; j<fts; j++) {
                if(ft[j]==ft[fts]) {
                    dupe=true;
                    break;
                }
            }
            if(!dupe) {
                ++fts;
            }
        }
    }

    uint32_t i,k;
    uint32_t temp;

    n=fts;
    for(i = 0; i<n-1; i++) {
        for(k = 0; k<n-1-i; k++) {
            if(ft[k] > ft[k+1]) {
                temp = ft[k];
                ft[k] = ft[k+1];
                ft[k+1] = temp;
            }
        }
    }
    pc=(char*)&ft[0];
    sti=0;
    for(i=0; i<fts; i++) {
        //printf("%02d : %d %02x %02x %02x %02x\n",i,ft[i],pc[0],pc[1],pc[2],pc[3]);
        ftc[0]=pc[2];
        ftc[1]=pc[1];
        ftc[2]=pc[0];
        ftc[3]=pc[3];
        printf("S: %02x %02x %02x %s\n",ftc[0],ftc[1],ftc[2],ftc);
        ftst[sti+0]=ftc[0];
        ftst[sti+1]=ftc[1];
        ftst[sti+2]=ftc[2];
        ftst[sti+3]='\n';
        pc+=4;
        sti+=4;
    }
    ftst[sti]=0;
    printf("CHARLIST:\n%s\n",ftst);
    cn_chars=fts;
}


bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

#define AHT15 0x38
#define I2C0 i2c0
#define I2C_SDA 4
#define I2C_SCL 5

void i2c_scan() {

    i2c_init(I2C0, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }
        int ret;
        uint8_t rxdata=0;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(I2C0, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
        //if(ret>=0 && addr==HIH71){has_hih=true;}
    }
}

void i2c_read() {
    uint8_t cmd=0;
    uint8_t data[6];

    //cmd = 0b11100001; // init
    cmd = 0xE1; // reset
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);

    //cmd = 0b10101100; // read
    cmd = 0xAC;
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);
    i2c_read_blocking( I2C0, AHT15, &data[0],6, false);
    printf("0x%02x 0x%02x\n",data[0],data[1]);
    printf("0x%02x 0x%02x\n",data[2],data[3]);
    printf("0x%02x 0x%02x\n",data[4],data[5]);

    //uint32_t humidity   = data[1];                          //20-bit raw humidity data
    //         humidity <<= 8;
    //         humidity  |= data[2];
    //         humidity <<= 4;
    //         humidity  |= data[3] >> 4;
    //if (humidity > 0x100000) {humidity = 0x100000;}             //check if RH>100, no need to check for RH<0 since "humidity" is "uint"
    //float hum = ((float)humidity / 0x100000) * 100;

    //uint32_t temperature   = data[3] & 0x0F;                //20-bit raw temperature data
    //         temperature <<= 8;
    //         temperature  |= data[4];
    //         temperature <<= 8;
    //         temperature  |= data[5];

    //float tem = ((float)temperature / 0x100000) * 200 - 50;
    //printf("%f %f\n",hum,tem);

}

#define MS 1000
#define US 1000000
#define BUTD 500  // delay between possible button presses (default: 500, half of a second)
#define REBOOT US*3 // 30 second/10
uint32_t rebootcounter = 0;
uint32_t rebootcounterold = 0;
uint32_t button0_time=0;
uint32_t button1_time=0;
uint32_t button0_dif=0;
uint32_t button1_dif=0;

void gpio_callback(uint gpio, uint32_t events) {
    if(events&GPIO_IRQ_EDGE_RISE) {
        if(gpio==CSW) {
            osw=true;
        }
        if(gpio==CCLK) {
            gch='c';
        }
        if(gpio==CDT) {
            gch='d';
        }
        if(gpio==CBUT0) {
            ceasefire=true;
            fire_pressed=false;
            rebootcounter=0;
        }
        if(gpio==CBUT1) {
            ceasefire=true;
            fire_pressed=false;
            rebootcounter=0;
        }

        gbuf[0]=gbuf[1];
        gbuf[1]=gch;
    }

    if(events&GPIO_IRQ_EDGE_FALL) {
        if(gpio==CSW) {
            sw=true;
        }
        if(gpio==CCLK) {
            gch='C';
        }
        if(gpio==CDT) {
            gch='D';
        }
        //if(gpio==CBUT0){
        //  printf("tus: %d\n",time_us_32());
        //  printf("b0t: %d\n",button0_time);
        //}
        if(gpio==CBUT0 && !fire && (((time_us_32()-button0_time)/MS)>=BUTD)) {
            ceasefire=false;
            fire=true;
            button0_time = time_us_32();
            fire_pressed=true;
        }
        if(gpio==CBUT1 && !fire && (((time_us_32()-button1_time)/MS)>=BUTD)) {
            ceasefire=false;
            fire=true;
            button1_time = time_us_32();
            fire_pressed=true;
        }
        gbuf[0]=gbuf[1];
        gbuf[1]=gch;
    }

    //if(events&GPIO_IRQ_LEVEL_LOW && gpio==CBUT0){
    //  buttonglass-=BUTTONGLASSC;
    //  if(buttonglass<=0){
    //    fire=true;
    //    if(plosa->editpos == 0){rebootcounter++;}
    //    buttonglass=BUTTONGLASS;
    //  }
    //}

    //if(events&GPIO_IRQ_LEVEL_LOW && gpio==CBUT1){
    //  buttonglass-=BUTTONGLASSC;
    //  if(buttonglass<=0){
    //    fire=true;
    //    if(plosa->editpos == 0){rebootcounter++;}
    //    buttonglass=BUTTONGLASS;
    //  }
    //}

    if(gbuf[0]=='C'&&gbuf[1]=='D') {
        tcw=true;
    }
    if(gbuf[0]=='D'&&gbuf[1]=='C') {
        tccw=true;
    }
    if(sw) {
        sw=false;
        fire=true;
    }
    if(osw) {
        osw=false;
        ceasefire=true;
    }


}
char C_SET[4]="set ";
char C_GET[4]="get ";
extern uint16_t drawlines;
extern int16_t Xadd;
extern int16_t Yadd;
void command(char* c) {
    bool tc=false;  // time changed
    char* left=c;
    if(strstr(left," ")) {
        char* space = strstr(left," ");
        space[0] = 0;
        char* right = space+1;
        if(strstr(left,"b2s")) {
            b2s = (int16_t)atoi(right);
        }
        if(strstr(left,"xa")) {
            Xadd = (int16_t)atoi(right);
        }
        if(strstr(left,"ya")) {
            Yadd = (int16_t)atoi(right);
        }
        if(strstr(left,"dither")) {
            plosa->dither = (bool)atoi(right);
        }
        if(strstr(left,"scandir")) {
            plosa->scandir = ((uint8_t)atoi(right))&0x03;
            lcd_setatt(plosa->scandir);
        }
        if(strstr(left,"ori")) {
            plosa->scandir = ((uint8_t)atoi(right))&0x03;
            lcd_setatt(plosa->scandir);
        }
        if(strstr(left,"sensors")) {
            plosa->sensors = (bool)atoi(right);
        }
        if(strstr(left,"gyro")) {
            plosa->gyrocross = (bool)atoi(right);
        }
        if(strstr(left,"bender")) {
            plosa->bender = (bool)atoi(right);
        }
        if(strstr(left,"smooth")) {
            plosa->SMOOTH_BACKGROUND = (bool)atoi(right);
        }
        if(strstr(left,"insomnia")) {
            plosa->INSOMNIA = (bool)atoi(right);
        }
        if(strstr(left,"circle")) {
            plosa->DYNAMIC_CIRCLES = (bool)atoi(right);
        }
        if(strstr(left,"light")) {
            plosa->BRIGHTNESS = (uint8_t)atoi(right);
            lcd_set_brightness(plosa->BRIGHTNESS);
        }
        if(strstr(left,"deep")) {
            plosa->DEEPSLEEP = (bool)atoi(right);
        }
        if(strstr(left,"high")) {
            plosa->highpointer = (bool)atoi(right);
        }
        if(strstr(left,"alpha")) {
            plosa->alphapointer = (bool)atoi(right);
        }
        if(strstr(left,"pstyle")) {
            plosa->pstyle = (PSTYLE)atoi(right);
        }
        if(strstr(left,"texture")) {
            plosa->texture = (uint8_t)atoi(right);
            if(plosa->texture>=TEXTURES) {
                plosa->texture=TEXTURES-1;
            }
        }
        if(strstr(left,"editpos")) {
            plosa->editpos = (uint8_t)atoi(right);
            if(plosa->editpos>EDITPOSITIONS) {
                plosa->editpos=EDITPOSITIONS;
            }
            repos(plosa->editpos);
        }
        if(strstr(left,"spin")) {
            plosa->spin = (int16_t)atoi(right);
        }
        if(strstr(left,"clock")) {
            plosa->clock = (bool)atoi(right);
        }
        if(strstr(left,"pointerdemo")) {
            plosa->pointerdemo = (bool)atoi(right);
        }
        if(strstr(left,"pd")) {
            plosa->pointerdemo = (bool)atoi(right);
        }
        if(strstr(left,"bg")) {
            plosa->conf_bg = (uint8_t)atoi(right);
            if(plosa->conf_bg >= MAX_BG) {
                plosa->conf_bg=0;
            }
        }
        if(strstr(left,"theme")) {
            plosa->theme = (uint8_t)atoi(right);
            if(plosa->theme>=THEMES) {
                plosa->theme=THEMES-1;
            }
        }
        if(strstr(left,"deg")) {
            flagdeg=(int16_t)atoi(right);
        }
        if(strstr(left,"blit")) {
            int16_t d = (int16_t)atoi(right);
            Vec2 dp1 = {128,20};
            Vec2 dp0 = {102,20};
            draw_pointer_mode(dp0,dp1,d, colt[plosa->theme]->col_m,textures[plosa->texture],BLACK,PS_TEXTURE);
        }
        if(strstr(left,"hour")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>=0&&tv<24) {
                plosa->dt.hour = tv;
                tc=true;
            }
        }
        if(strstr(left,"min")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>=0&&tv<60) {
                plosa->dt.min  = tv;
                tc=true;
            }
        }
        if(strstr(left,"sec")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>=0&&tv<60) {
                plosa->dt.sec  = tv;
                tc=true;
            }
        }

        if(strstr(left,"day")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>0&&tv<=last[plosa->dt.month]) {
                plosa->dt.day = tv;
                tc=true;
            }
        }
        if(strstr(left,"mon")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>0&&tv<13) {
                plosa->dt.month  = tv;
                tc=true;
            }
        }
        if(strstr(left,"year")) {
            uint16_t tv = (uint16_t)atoi(right);
            if(tv>=0) {
                plosa->dt.year  = tv;
                tc=true;
            }
        }
        if(strstr(left,"dotw")||strstr(left,"WDAY")) {
            uint8_t tv = (uint8_t)atoi(right);
            if(tv>=0&&tv<7) {
                plosa->dt.dotw  = tv;
                tc=true;
            }
        }
        if(tc) {
            rtc_set_datetime(&plosa->dt);
        }
        return;
    }
    if(strstr(left,"cir0")) {
        plosa->DYNAMIC_CIRCLES=false;
        printf("DYCI0\n");
        return;
    }
    if(strstr(left,"cir1")) {
        plosa->DYNAMIC_CIRCLES=true;
        printf("DYCI1\n");
        return;
    }
    if(strstr(left,"batmax")) {
        printf("BATMAX: %f\n", plosa->bat.max);
        return;
    }
    if(strstr(left,"batmin")) {
        printf("BATMIN: %f\n", plosa->bat.min);
        return;
    }
    if(strstr(left,"save")) {
        dosave();
    }
    if(strstr(left,"deg+")) {
        flagdeg++;
    }
    if(strstr(left,"deg-")) {
        flagdeg--;
    }

    //if(strstr(left,"read")){ uint8_t data[4]; i2c_read((uint8_t*)&data); }
    //if(strstr(left,"scan")){ i2c_scan(); }
    if(strstr(left,"rota")) {
        plosa->gfxmode=GFX_ROTATE;
    }
    if(strstr(left,"roto")) {
        plosa->gfxmode=GFX_ROTOZOOM;
    }
    if(strstr(left,"norm")) {
        plosa->gfxmode=GFX_NORMAL;
    }
    if(strstr(left,"stat")) {
        if(plosa->theme>=THEMES) {
            plosa->theme=0;
        }
        if(plosa->editpos>8) {
            plosa->editpos=0;
        }
        if(plosa->dt.dotw>6) {
            plosa->dt.dotw=0;
        }

        printf("\n- STATUS -\n");
        printf("mode: %s\n", plosa->mode);
        printf("date: %s %02d.%02d.%04d\n", week[plosa->theme][plosa->dt.dotw], plosa->dt.day, plosa->dt.month, plosa->dt.year);
        printf("time: %02d:%02d:%02d\n", plosa->dt.hour, plosa->dt.min, plosa->dt.sec);
        printf("battery: %s %.2fmA load: %d min: %.2f max: %.2f read: %.2f\n", plosa->bat.mode, plosa->bat.mA, (plosa->bat.load) ? 1 : 0, plosa->bat.min, plosa->bat.max, plosa->bat.read);
        printf("theme: %d\n", plosa->theme);
        printf("editpos: %d\n", plosa->editpos);
        printf("brightness: %d\n", plosa->BRIGHTNESS);
        printf("sleeping: %s\n", (plosa->is_sleeping)?"1":"0");
        printf("sensors: %s\r\n", plosa->sensors?"1":"0");
        printf("gyrocross: %s\n", plosa->gyrocross?"1":"0");
        printf("smooth: %s\n", plosa->SMOOTH_BACKGROUND?"1":"0");
        printf("insomnia: %s\n", plosa->INSOMNIA?"1":"0");
        printf("circles: %s\n", plosa->DYNAMIC_CIRCLES?"1":"0");
        printf("deepsleep: %s\n", plosa->DEEPSLEEP?"1":"0");
        printf("highpointer: %s\n", plosa->highpointer?"1":"0");
        //printf("%s\n", crcstatus);
        printf("%s\n", flashstatus);
    }

    // Reboot
    if (strstr(left, "reboot")) reset_usb_boot(0, 0);

    if(strstr(left,"SNAPSHOT")) {
        //printf("-----------------------> CUT HERE <---------------------\n\nuint8_t imagedata[138+  240*240*2] = {\n");
        if(b0==NULL) {
            return;
        }
        uint8_t snaphead[] = {
            //  ID   ||  SIZE  |                                                                     | WIDTH   |          | HEIGHT |
            0x4d,0x42,0xc2,0x8a,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x8a,0x00,0x00,0x00,0x7c,0x00,0x00,0x00,0xf0,0x00,0x00,0x00,0xf0,
            0x00,0x00,0x00,0x01,0x00,0x10,0x00,0x03,0x00,0x00,0xc2,0x00,0x00,0x01,0x0b,0x12,0x00,0x00,0x0b,0x12,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x00,0x00,0x00,0x07,0xe0,0x00,0x00,0x00,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x47,0x42,
            0x73,0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        };
        for(uint32_t i=0; i<138; i+=2) {
            putchar_raw(snaphead[i+1]);
            putchar_raw(snaphead[i]);
        }

        for(uint32_t i=0; i<LCD_SZ; i+=2) {
            putchar_raw(b0[i+1]);
            putchar_raw(b0[i]);
        }
        stdio_flush();
        //stdio_usb_init();
    }
}

void shell() {
    // shell
    comt=getchar_timeout_us(100);
    while(comt!=PICO_ERROR_TIMEOUT) {
        comc=comt&0xff;
        //putchar(comc);
        combufa[comi++]=comc;
        if(comc=='\n') {
            combufa[comi]=0;
            combufa[comi-1]=0;
            //printf("CMD: %s\n",combufa);
            command(&combufa[0]);
            comi=0;
        }
        if(comi==254) {
            comi=0;
        }
        comt=getchar_timeout_us(100);
    }
}

int16_t get_acc02f(float f0, float f1, float FACT) {
    switch(plosa->scandir) {
    case 0:
        return (int16_t)(f0/FACT);
        break;
    case 1:
        return (int16_t)(f1/FACT);
        break;
    case 2:
        return (int16_t)(f0/-FACT);
        break;
    case 3:
        return (int16_t)(f1/-FACT);
        break;
    }
}

int16_t get_acc12f(float f0, float f1, float FACT) {
    switch(plosa->scandir) {
    case 0:
        return (int16_t)(f1/FACT);
        break;
    case 1:
        return (int16_t)(f0/-FACT);
        break;
    case 2:
        return (int16_t)(f1/-FACT);
        break;
    case 3:
        return (int16_t)(f0/FACT);
        break;
    }
}

int16_t get_acc02(float f0, float f1) {
    return get_acc02f(f0,f1,25.0f);
}
int16_t get_acc12(float f0, float f1) {
    return get_acc12f(f0,f1,25.0f);
}

int16_t get_acc0() {
    return get_acc02(acc[0],acc[1]);
}
int16_t get_acc1() {
    return get_acc12(acc[0],acc[1]);
}

int16_t draw_getdeg(int16_t deg) {
    float   FACT      = 25.0f;
    int16_t EPC_BXY   = 50;
    int16_t FINE_STOP = 2;
    //int8_t xa = (int8_t)(acc[0]/FACT);
    //int8_t ya = (int8_t)(acc[1]/FACT);
    int8_t xa = (int8_t)get_acc0();
    int8_t ya = (int8_t)get_acc1();
    //printf("{%d} %d %d -> %d %d / %d %d [%d %d] %d %d",plosa->configpos,xa,ya,hgx,hgy,(int8_t)(hgx/FACT),(int8_t)(hgy/FACT));
    xa = xa - (int8_t)(hgx/FACT);
    ya = ya - (int8_t)(hgy/FACT);
    if(xa>  EPC_BXY) {
        xa=  EPC_BXY;
    }
    if(xa< -EPC_BXY) {
        xa= -EPC_BXY;
    }
    if(ya>  EPC_BXY) {
        ya=  EPC_BXY;
    }
    if(ya< -EPC_BXY) {
        ya= -EPC_BXY;
    }
    //printf("%d %d  , %d %d\n",xa,ya,deg);
    if(ya>2||ya<-2) {
        deg+=ya;
    }
    return chkdeg(deg);
}

void draw_init() {
    doi_config = DOImage_new(240-(32+16), 120-16, 32,32, BLACK, config);
    doi_config_cn = DOImage_new(18, 120-16, 32,32, BLACK, config);
}

void fx_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t c, uint16_t ps, uint16_t xo, uint16_t yo) {
    if(plosa->DYNAMIC_CIRCLES) {
        lcd_bez3circ(x,y,r,c,ps,xo,yo);
    } else {
        lcd_circle(x,y,r,c,ps,0);
    }
}

#define CIRCMENU_RADIUS 88

int16_t draw_circmenu(int16_t cdf, uint8_t num_items, const uint8_t** src_menuitems) {
    int16_t cdeg_fine=cdf;
    int16_t cdeg = cdeg_fine;
    int16_t cdegc;
    int16_t maxcd = (int16_t)(DEGS/num_items);
    if(plosa->theme==0) {
        cdegc = DEGS-cdeg_fine-QDEG;
    } else {
        cdegc = DEGS-cdeg_fine-3*QDEG;
    }
    cdeg=chkdeg(cdeg);
    cdegc=chkdeg(cdegc);
    int16_t cdegd = cdegc%maxcd;
    plosa->configpos = cdegc/maxcd;
    if(cdegd > (maxcd/2)) {
        ++plosa->configpos;
    }
    //printf("configpos=%d cdegd=%d\n",plosa->configpos,cdegd);
    for(uint16_t i=0; i<num_items; i++) {
        Vec2 cv = gvdl(cdeg,CIRCMENU_RADIUS);
        lcd_blit(cv.x-16+LCD_W2,cv.y-16+LCD_H2,32,32,BLACK,src_menuitems[i]);
        cdeg=chkdeg(cdeg+maxcd);
    }
    int16_t d = (cdeg+90)%maxcd;
    if(d!=0) {
        if(d>(maxcd/2)) {
            return d-maxcd;
        } else {
            return d;
        }
    }
    return 0;

}

void draw_clock_hands() {
    uint8_t x1,y1,xt,yt;
    int xi,yi;
    uint8_t x0=120;
    uint8_t y0=120;
    float mindeg = 1024.0f/60.0f;
    Vec2 dp1 = texsize[plosa->texture];
    int16_t tu=(int16_t)plosa->dt.min*mindeg;
    Vec2 dp0 = {102,10};
    dp0 = psize_m[plosa->texture];
    if(plosa->pstyle==PS_NORMAL) {
        dp0 = vset(102,3);
    } else if(plosa->pstyle==PS_ALPHA) {
        dp0 = vset(102,4);
    }
    draw_pointer_mode(dp0,dp1,tu,colt[plosa->theme]->col_m,textures[plosa->texture],BLACK,plosa->pstyle);
    tu=(int16_t)plosa->dt.hour;
    if(tu>=12) {
        tu-=12;
    }
    tu=(int16_t)(tu*mindeg*5);
    tu+=(int16_t)((float)256/3/60*plosa->dt.min);
    dp0=vset(75, 20);
    dp0 = psize_h[plosa->texture];
    if(plosa->pstyle==PS_NORMAL) {
        dp0 = vset(65,6);
    } else if(plosa->pstyle==PS_ALPHA) {
        dp0 = vset(65,6);
    }
    draw_pointer_mode(dp0,dp1,tu, colt[plosa->theme]->col_h,textures[plosa->texture],BLACK,plosa->pstyle);

    tu=(int16_t)(plosa->dt.sec*mindeg);
    //if(!analog_seconds){
    // 'jump' seconds
    int16_t seci = ((int16_t)plosa->dt.sec*mindeg);
    xi = (int8_t)(tcos[seci]*114);
    yi = (int8_t)(tsin[seci]*114);
    x1 = (uint8_t)x0+xi;
    y1 = (uint8_t)y0+yi;
    if(plosa->bender==true) {
        int16_t xit=x1;
        int16_t yit=y1;
        xi = (int8_t)(tcos[seci]*106);
        yi = (int8_t)(tsin[seci]*106);
        x1 = (uint8_t)x0+xi;
        y1 = (uint8_t)y0+yi;
        Vec2 v1= {x1,y1};
        lcd_alpha_on();
        //lcd_bez2curve(0,0,(int8_t)(xi/2)+(int8_t)(acc[1]/25.0f),(int8_t)(yi/2)-(int8_t)(acc[0]/25.0f),xi,yi,114,colt[plosa->theme]->col_s,2);
        lcd_bez2curve(0,0,(int8_t)(xi/2),(int8_t)(yi/2),xi,yi,114,colt[plosa->theme]->col_s,2);
        lcd_alpha_off();
        if(plosa->pstyle == PS_ALPHA) {
            lcd_alpha_line_deg(v1, tu, 7, colt[plosa->theme]->col_s, 1);
        } else {
            lcd_line_deg(v1, tu, 7, colt[plosa->theme]->col_s, 1);
        }
    } else {
        dp0.x=114;
        dp0.y=1;
        draw_pointer_mode(dp0,dp0,tu, colt[plosa->theme]->col_s,textures[plosa->texture],BLACK,PS_NORMAL);
    }
    //lcd_blit((int)(x0-8+tcos[seci]*100),(int)(y0-8+tsin[seci]*100),16,16,colt[plosa->theme]->alpha,stars[plosa->theme]);
    //}
}

void draw_gfx() {
    if(!draw_gfx_enabled) {
        return;
    }
    uint8_t x1,y1,xt,yt;
    uint8_t x0=120;
    uint8_t y0=120;
    if(!plosa->clock) {
        return;
    }
    // battery display
//    lcd_frame(POS_BAT_X    ,POS_BAT_Y,   POS_BAT_X+102+(POS_BAT_PS<<1), POS_BAT_Y+POS_BAT_YS, BLUE, POS_BAT_PS); // frame
//    lcd_line(POS_BAT_X-1    ,POS_BAT_Y+1, POS_BAT_X-1    ,POS_BAT_Y+POS_BAT_YS-2,BLUE,1);// round end
//    lcd_line(POS_BAT_X+102+(POS_BAT_PS<<1)    ,POS_BAT_Y+1, POS_BAT_X+102+(POS_BAT_PS<<1)    ,POS_BAT_Y+POS_BAT_YS-2,BLUE,1); //round end
//    lcd_yline(POS_BAT_X+103+(POS_BAT_PS<<1)    ,POS_BAT_Y+POS_BAT_YS/2-2,4,__builtin_bswap16(BLUE),2);  //+
    //printf("bat: %f %f %f %f\n",plosa->bat.read,plosa->bat.max, plosa->bat.min, plosa->bat.diff);
    float bat_diff = ( plosa->bat.diff - (plosa->bat.max - plosa->bat.read));
    if (bat_diff < 0.0f) bat_diff = 0.0f;
    //printf("(%f)  %f %f [%f]\n",(resultsummid()*conversion_factor),plosa->bat.diff,bat_diff,(bat_diff/plosa->bat.diff)*100.0f);

    // Battery
    uint16_t bat = (uint16_t)((bat_diff/  plosa->bat.diff) * 100.0f);
    //printf("bat :  %03d\n",bat);
    if (bat > 100) bat = 100;

    uint16_t level_color = colt[plosa->theme]->bat_level;
    if(bat>10&&bat<30) {
        level_color = colt[plosa->theme]->bat_level_low;
    }
    if(bat<10) {
        level_color = colt[plosa->theme]->bat_level_critical;
    }
    //lcd_xline(POS_BAT_X+POS_BAT_PS    ,POS_BAT_Y+POS_BAT_PS,   bat+1, __builtin_bswap16(level_color), POS_BAT_YS-(POS_BAT_PS<<1)); // battery level
    if(!usb_loading) {
        sprintf(dbuf,"  %02d%%",bat);
    } else {
        sprintf(dbuf,"uWatch",bat);
    }
//    lcd_str(94, 12, dbuf, &Font12, level_color, BLACK);

    if(plosa->sensors) {
        if((plosa->dt.sec==0||plosa->dt.sec==30)&&(!temp_read)) {
            temperature = QMI8658_readTemp();
            temp_read=true;
        } else {
            temp_read=false;
        }
    }
    //sprintf(dbuf,"%d",flagdeg%90);
    //lcd_str(114, 42, dbuf, &Font20, YELLOW, BLACK);
    //printf("acc_x   = %4.3fmg , acc_y  = %4.3fmg , acc_z  = %4.3fmg\r\n", acc[0], acc[1], acc[2]);
    //printf("gyro_x  = %4.3fdps, gyro_y = %4.3fdps, gyro_z = %4.3fdps\r\n", gyro[0], gyro[1], gyro[2]);
    //printf("tim_count = %d\r\n", tim_count);
    int xi,yi;
    Vec2 vc_s,vc_e;
    float scf = DEGS/360.0f;
    float mindeg = 1024.0f/60.0f;
    for(int16_t i=0; i<60; i++) {
        vc_e = gvdl((int16_t)i*mindeg,119);
        vc_e = vadd(vc_e,vO);
        if(!(i%5)) {
            vc_s = gvdl((int16_t)i*mindeg,110);
            vc_s = vadd(vc_s,vO);
            lcd_linev2(vc_s,vc_e, colt[plosa->theme]->col_cs, 1);
        } else {
            vc_s = gvdl((int16_t)i*mindeg,115);
            vc_s = vadd(vc_s,vO);
            lcd_linev2(vc_s,vc_e, colt[plosa->theme]->col_cs5, 1);
        }
    }

    draw_clock_hands();
    if(plosa->pointerdemo) {
        for(uint16_t i=0; i<7; i++) {
            Vec2 vo = {120,120};
            uint16_t it=i;
            if(it>=TEXTURES) {
                it-=TEXTURES;
            }
            if(i&1) {
                lcd_blit_deg2(vo,psize_h[it],texsize[it],fdegs[i],textures[it],BLACK,false);
            } else {
                lcd_blit_deg2(vo,psize_m[it],texsize[it],fdegs[i],textures[it],BLACK,false);
            }

        }
    } else {
        //lcd_blit(120-16,120-16,32,32,colt[plosa->theme]->alpha, flags[plosa->theme]); // center
    }

    if(plosa->spin!=0) {
        flagdeg = gdeg(flagdeg+plosa->spin);
        flagdeg1  = gdeg(flagdeg1 +plosa->spin+(gyrox>>3));
        flagdeg2  = gdeg(flagdeg2 -plosa->spin*7);
        flagdeg1a = gdeg(flagdeg1a-plosa->spin*2);
        flagdeg2a = gdeg(flagdeg2a+plosa->spin*5);
        flagdeg1b = gdeg(flagdeg1b-plosa->spin);
        flagdeg2b = gdeg(flagdeg2b+plosa->spin*7);
        int i=0;
        fdegs[i]= gdeg(fdegs[i]+plosa->spin);
        ++i;
        fdegs[i]= gdeg(fdegs[i]+plosa->spin*9+(gyrox>>3));
        ++i;
        fdegs[i]= gdeg(fdegs[i]-plosa->spin*7);
        ++i;
        fdegs[i]= gdeg(fdegs[i]-plosa->spin*2);
        ++i;
        fdegs[i]= gdeg(fdegs[i]+plosa->spin*5);
        ++i;
        fdegs[i]= gdeg(fdegs[i]-plosa->spin*3);
        ++i;
        fdegs[i]= gdeg(fdegs[i]+plosa->spin*7);
        ++i;

    }
    // graphical view of x/y gyroscope
    if(plosa->gyrocross) {
#define GSPX 120
#define GSPY 200
#define GSPS 4
#define GSPSZ 20

//        lcd_frame(GSPX-GSPS , GSPY-GSPSZ, GSPX+GSPS, GSPY+GSPSZ,WHITE,1); //vert |
//        lcd_frame(GSPX-GSPSZ, GSPY-GSPS, GSPX+GSPSZ,GSPY+GSPS, WHITE,1); //horz –
//    float fx = (acc[1]/25.0f); // -20 – 20
//    float fy = (acc[0]/25.0f);
        float fy = get_acc0();
        float fx = get_acc1();

        if(hg_enabled) {
            fy -= (int8_t)get_acc02(hgx,hgy);
            fx -= (int8_t)get_acc12(hgx,hgy);
        }
        //printf("gxy: %f %f\n",fx,fy);
        int8_t gx = (int8_t)fx;
        int8_t gy = (int8_t)fy;
        if(gx>  (GSPSZ-GSPS)) {
            gx= (GSPSZ-GSPS);
        }
        if(gx< -(GSPSZ-GSPS)) {
            gx=-(GSPSZ-GSPS);
        }
        if(gy>  (GSPSZ-GSPS)) {
            gy= (GSPSZ-GSPS);
        }
        if(gy< -(GSPSZ-GSPS)) {
            gy=-(GSPSZ-GSPS);
        }
        //printf("gxy: %d %d\n",gx,gy);
        uint16_t gdx = (uint16_t)GSPX+gx;
        uint16_t gdy = (uint16_t)GSPY-gy;
        uint16_t gcoly = __builtin_bswap16(WHITE);
        uint16_t gcolx = __builtin_bswap16(YELLOW);
//        lcd_pixel_rawps(GSPX,gdy,gcoly,GSPS);
//        lcd_pixel_rawps(gdx,GSPY,gcolx,GSPS);
        if(hg_enabled) {
            gy = (int8_t)get_acc02(hgx,hgy); //(hgx/25.0f);
            gx = (int8_t)get_acc12(hgx,hgy); //(hgy/25.0f);
            if(gx>  (GSPSZ-GSPS)) {
                gx= (GSPSZ-GSPS);
            }
            if(gx< -(GSPSZ-GSPS)) {
                gx=-(GSPSZ-GSPS);
            }
            if(gy>  (GSPSZ-GSPS)) {
                gy= (GSPSZ-GSPS);
            }
            if(gy< -(GSPSZ-GSPS)) {
                gy=-(GSPSZ-GSPS);
            }
            //printf("gxy: %d %d\n",gx,gy);
            gdx = (uint16_t)GSPX+gx;
            gdy = (uint16_t)GSPY-gy;
            int16_t GSPSs = GSPS-1;
//            lcd_frame(GSPX-GSPSs,gdy -GSPSs,GSPX + GSPSs,gdy  +GSPSs,LGRAY,1);
//            lcd_frame(gdx -GSPSs,GSPY-GSPSs,gdx  + GSPSs,GSPY +GSPSs,ORANGE,1);
        }
    }
}

void draw_text() {
    // Draw text
    if (!draw_text_enabled) return;

    // Draw sensors
    if (plosa->sensors) {
        lcd_str(POS_ACC_X, POS_ACC_Y+  1, "GYR_X =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+ 15, "GYR_Y =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+ 48, "GYR_Z =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+ 98, "ACC_X =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+130, "ACC_Y =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+144, "ACC_Z =", &Font12, WHITE, BLACK);
        lcd_str(POS_ACC_X, POS_ACC_Y+156, "TEMP", &Font12, WHITE, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+  1,  acc[0], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+ 15,  acc[1], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+ 48,  acc[2], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+ 98, gyro[0], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+130, gyro[1], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+144, gyro[2], &Font12, YELLOW, BLACK);
        lcd_float(POS_ACC_X+70, POS_ACC_Y+156, temperature, &Font12,  YELLOW, BLACK);
        lcd_str(50, 208, "BAT(V)", &Font16, WHITE, BLACK);
        lcd_floatshort(130, 208, resultsummid(), &Font16, ORANGE, BLACK);
    }

    // Day of the week
    lcd_str(POS_DOW_X, POS_DOW_Y, week[plosa->theme][plosa->dt.dotw], &TFONT, colors[0], BLACK);

    // Day
    uint8_t yoff_date = POS_DATE_Y;
    uint8_t yoff_time = POS_TIME_Y;
    sprintf(dbuf,"%2d", plosa->dt.day);
    lcd_str(POS_DATE_X+0*TFW, yoff_date, dbuf, &TFONT, colors[1], BLACK);
    lcd_str(POS_DATE_X+2*TFW, yoff_date, ".", &TFONT, WHITE, BLACK);

    // Month
    sprintf(dbuf,"%2d", plosa->dt.month);
    lcd_str(POS_DATE_X+3*TFW, yoff_date, dbuf, &TFONT, colors[2], BLACK);
    lcd_str(POS_DATE_X+5*TFW, yoff_date, ".", &TFONT, WHITE, BLACK);

    // Year
    sprintf(dbuf,"%04d", plosa->dt.year);
    lcd_str(POS_DATE_X+6*TFW, yoff_date, dbuf, &TFONT, colors[3], BLACK);

    // Hour
    int hour = plosa->dt.hour;
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;
    sprintf(dbuf, "%2d", hour);
    lcd_str(POS_TIME_X,       yoff_time, dbuf, &Font24, colors[4], BLACK);
    lcd_str(POS_TIME_X+2*TFW, yoff_time, ":", &Font24, WHITE, BLACK);

    // Min
    sprintf(dbuf,"%02d", plosa->dt.min);
    lcd_str(POS_TIME_X+3*TFW, yoff_time, dbuf, &Font24, colors[5], BLACK);
    lcd_str(POS_TIME_X+5*TFW, yoff_time, ":", &Font24, WHITE, BLACK);

    // Sec
    sprintf(dbuf,"%02d", plosa->dt.sec);
    lcd_str(POS_TIME_X+6*TFW, yoff_time, dbuf, &Font24, colors[6], BLACK);
}

int main(void) {
    // Save to flash
    plosa->dummy = 0;
    if (strstr((char*)plosa->mode, "SAVE")) {
        // Save
        sprintf((char*)plosa->mode, "LOAD");
        plosa->save_crc = crc(&plosa->theme, LOSASIZE);
        flash_data();
        sprintf(flashstatus, "flash: saved\0");
    } else {
        // Load from flash
        if(!force_no_load && !strstr((char*)plosa->mode, "LOAD")) {
            flash_data_load();
            sprintf(flashstatus,"flash: loaded\0");
        } else {
            sprintf(flashstatus,"flash: normal\0");
        }
    }

    // Init
    stdio_init_all();
    sleep_ms(400);

    // Check storage
    check_save_data();

    // Init screen
    lcd_init();
    lcd_setatt(plosa->scandir&0x03);
    lcd_make_cosin();
    draw_init();
    lcd_set_brightness(plosa->BRIGHTNESS);

    // Log
    printf("%02d-%02d-%04d %02d:%02d:%02d [%d]\n", plosa->dt.day, plosa->dt.month, plosa->dt.year, plosa->dt.hour, plosa->dt.min, plosa->dt.sec, plosa->dt.dotw);
    printf("mode='%s'\n", plosa->mode);
    printf("%s\n", crcstatus);
    printf("%s\n", flashstatus);

    // Allocate screen image
    b0 = malloc(LCD_SZ);
    if (b0 == 0) printf("b0==0!\n");
    lcd_setimg((uint16_t*)b0);

    // Themes
    colt[0] = &colt2;
    colt[1] = &colt2;
    colt[2] = &colt2;
    colt[3] = &colt2;
    colt[4] = &colt2;
    colt[5] = &colt2;

    // Init realtime clock
    gpio_set_dir(CBUT0,GPIO_IN);
    gpio_pull_up(CBUT0);
    gpio_set_dir(CBUT1,GPIO_IN);
    gpio_pull_up(CBUT1);
    gpio_set_irq_enabled_with_callback(CCLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(CDT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CSW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CBUT0, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CBUT1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    rtc_init();
    rtc_set_datetime(&plosa->dt);

    // Leap year
    if (!(plosa->dt.year % 4)) last[2] = 29;
    else last[2] = 28;

    // IMU
    QMI8658_init();
    printf("QMI8658_init\r\n");
    acc[0] = 0.0f;
    acc[1] = 0.0f;
    acc[2] = 0.0f;

    // Fonts
    print_font_table();

    // Stats
    command("stat");

    // Main loop
    bool qmis = false;
    while(true) {

        // Read IMU
        qmis = !qmis;
        if(qmis) QMI8658_read_xyz(acc, gyro, &tim_count);

        // Check if not moving
        #define GYRMAX 300.0f
        #define ACCMAX 500.0f
        bool no_moveshake = false;
        if(theme_bg_dynamic_mode==1) {
            if((gyro[0]>-ACCMAX&&gyro[0]<ACCMAX)&&(gyro[1]>-ACCMAX&&gyro[1]<ACCMAX)&&(gyro[2]>-ACCMAX&&gyro[2]<ACCMAX)) {
                no_moveshake = true;
            }
        } else {
            if((acc[0]>-GYRMAX&&acc[0]<GYRMAX)&&(acc[1]>-GYRMAX&&acc[1]<GYRMAX)) {
                no_moveshake = true;
            }
        }
        if(acc[2]>=0.0f) { //force awake!
            no_moveshake=true;
            screensaver=SCRSAV;
            plosa->is_sleeping=false;
            theme_bg_dynamic_mode = 0;
            lcd_set_brightness(plosa->BRIGHTNESS);
            lcd_sleepoff();
        }
        //if((acc[2]>0&&last_z<0)||(acc[2]<0&&last_z>0)){no_moveshake=true;}  // coin-flipped
        last_z=acc[2];
        if(no_moveshake) {
            if(!plosa->is_sleeping && cmode==CM_None && !(usb_loading|plosa->INSOMNIA)) {
                screensaver--;
                if(screensaver<=0) {
                    if(bg_dynamic[plosa->conf_bg]) {
                        theme_bg_dynamic_mode++;
                        if(theme_bg_dynamic_mode==1) {
                            screensaver=SCRSAV2;
                            continue;
                        }
                    }
                    plosa->is_sleeping=true;
                    screensaver=SCRSAV;
                    lcd_set_brightness(0);
                    lcd_sleepon();
                }
            }
        } else {
            if(plosa->is_sleeping) {
                plosa->is_sleeping=false;
                lcd_set_brightness(plosa->BRIGHTNESS);
                lcd_sleepoff();
                sleep_frame=SLEEP_FRAME_START;
            }
            if(theme_bg_dynamic_mode) {
                theme_bg_dynamic_mode--;
            }
        }

        if(plosa->is_sleeping) {
            sleep_ms(sleep_frame);
            if(plosa->DEEPSLEEP) {
                if(sleep_frame<SLEEP_FRAME_END) {
                    sleep_frame+=SLEEP_FRAME_ADD;
                }
            }
            continue;
        }

        if(fire_pressed) {
            uint32_t t = time_us_32();
            if(button0_time) {
                button0_dif = t-button0_time;
            }
            if(button1_time) {
                button1_dif = t-button1_time;
            }
            if((button0_dif||button1_dif)&&(plosa->editpos==EPOS_CENTER)) { // only from central position (flag)
                //printf("%d %d %d\n",t,button0_time,button0_dif);
                if(button0_dif>=US) {
                    printf("REBOOT: [%d->%d] s\n", button0_dif/MS, REBOOT/MS);
                }
                if(button1_dif>=US) {
                    printf("REBOOT: [%d->%d] s\n", button1_dif/MS, REBOOT/MS);
                }
                if(button0_dif>=REBOOT || button1_dif >= REBOOT) {
                    printf("SAVING...\n");
                    dosave();
                }
            }
        }

        for(int i=0; i<LCD_SZ; i++) {
            b0[i]=0x00;
        }

        if(plosa->gfxmode==GFX_NORMAL||plosa->gfxmode==GFX_ROTATE) {
            if(bg_dynamic[plosa->conf_bg]) { // dynamic background
//          int8_t xa = (int8_t)(acc[1]/50.0f);
//          int8_t ya = (int8_t)(acc[0]/50.0f);
                int16_t ya = (int16_t)get_acc02f(acc[0],acc[1],50.0f); //(acc[1]/50.0f);
                int16_t xa = (int16_t)get_acc12f(acc[0],acc[1],50.0f); //(acc[0]/50.0f);
                if(xa>EYE_MAX) {
                    xa=EYE_MAX;
                }
                if(xa<-EYE_MAX) {
                    xa=-EYE_MAX;
                }
                if(ya>EYE_MAX) {
                    ya=EYE_MAX;
                }
                if(ya<-EYE_MAX) {
                    ya=-EYE_MAX;
                }
                if(plosa->SMOOTH_BACKGROUND) {
                    xoldt = xa;
                    yoldt = ya;
                    xa+=xold;
                    ya+=yold;
                    xa>>=1;
                    ya>>=1;
                    xold = xoldt;
                    yold = yoldt;
                }
                if(xa >15) {
                    xa= 15;
                }
                if(ya >15) {
                    ya= 15;
                }
                if(xa<-15) {
                    xa=-15;
                }
                if(ya<-15) {
                    ya=-15;
                }
                gyrox=xa;
                gyroy=ya;
                if(plosa->gfxmode==GFX_ROTATE) {
                    //printf("xya: %d %d\n",xa,ya);
                    //printf("XAYA: %d %d\n",xa,ya);
                    Vec2 vbo = {120+xa,120-ya};
                    //Vec2 vbs = {0,0};
                    Vec2 vbsz = {190,190};
                    //lcd_blit_deg(vbs,vbe,vbe,flagdeg,backgrounds[plosa->conf_bg],colt[plosa->theme]->alpha,true);
                    Vec2 vbuv = {190,190};
                    //lcd_blit_deg2(vbo,vbuv,vbsz,flagdeg,backgrounds[plosa->conf_bg],colt[plosa->theme]->alpha,true);

                    //Vec2 vbs_b= {180+xa,70-ya};
                    //vbs2.x = -35;
                    //vbe2.x =  25;
                    //lcd_blit_deg2(vbs2,vbe2,vbe,vbs_b,flagdeg,backgrounds[0],colt[plosa->theme]->alpha);
                    //if(plosa->dither==1){              lcd_dither(EYE_X+xa,EYE_Y-ya,EYE_SZ);            }
                } else {
                    //lcd_blit(EYE_X+xa,EYE_Y-ya,EYE_SZ,EYE_SZ,BLACK,backgrounds[plosa->conf_bg]);
                }
            } else {
                mcpy(b0,backgrounds[plosa->conf_bg],LCD_SZ);
            }
        } else if(plosa->gfxmode==GFX_ROTOZOOM) {
            lcd_roto(backgrounds[plosa->conf_bg],bg_size[plosa->conf_bg],bg_size[plosa->conf_bg]);
            lcd_rotoa();
        }

        uint8_t save_sec = plosa->dt.sec;
        uint8_t save_min = plosa->dt.min;
        if(cmode!=CM_Editpos || plosa->editpos==EPOS_CENTER ) {
            rtc_get_datetime(&plosa->dt);
        }
        //if(!fire && cmode==CM_Editpos && (plosa->editpos==6||plosa->editpos==5)){
        //  plosa->dt.sec = save_sec;
        //  plosa->dt.min = save_min;
        //}
        ++resulti;
        resulti&=0x0f;
        result[resulti] = read_battery();
        usb_loading = (resultsummid()>=plosa->bat.load);

        if(fire==true) {
            fire_counter++;
            //printf("fire! [%08x] (%d %d %d) {%d}\n",fire_counter,time_us_32()/MS,button0_time/MS,button1_time/MS,(time_us_32()-button0_time)/MS);

            sleep_frame = SLEEP_FRAME_START;
            plosa->is_sleeping = false;
            theme_bg_dynamic_mode=0;
            dir_x = D_NONE;
            dir_y = D_NONE;

            if(cmode==CM_None) {
                hgx = (int16_t)get_acc02f(acc[0],acc[1],1.0f);
                hgy = (int16_t)get_acc12f(acc[0],acc[1],1.0f);
                hg_enabled = true;
                cmode=CM_Changepos;
                colors[plosa->editpos]=edit_colors[plosa->theme];
            } else if(cmode==CM_Config) {
                if(draw_config_enabled==true) {
                    switch(plosa->configpos) {
                    case CP_EXIT:
                        cmode=CM_None;
                        draw_gfx_enabled=true;
                        draw_text_enabled=true;
                        draw_config_enabled=false;
                        hg_enabled = false;
                        break;
                    case CP_BACKGROUND:
                        ++plosa->conf_bg;
                        if(plosa->conf_bg==MAX_BG) {
                            plosa->conf_bg=0;
                        }
                        break;
                    case CP_PENSTYLE:
                        if(plosa->pstyle!=PS_TEXTURE) {
                            plosa->pstyle++;
                        } else {
                            plosa->pstyle=PS_NORMAL;
                        }
                        break;
                    case CP_ROTOZOOM:
                        plosa->rotoz = (bool)!plosa->rotoz;
                        if(plosa->rotoz) {
                            plosa->gfxmode=GFX_ROTOZOOM;
                        } else {
                            plosa->gfxmode=GFX_NORMAL;
                        }
                        break;
                    case CP_ROTATION:
                        plosa->rota = (bool)!plosa->rota;
                        if(plosa->rota) {
                            plosa->gfxmode=GFX_ROTATE;
                            if(plosa->spin==0) {
                                plosa->spin=1;
                            }
                        } else {
                            plosa->spin=0;
                            plosa->gfxmode=GFX_NORMAL;
                        }
                        break;
                    case CP_WAND:
                        plosa->texture++;
                        if(plosa->texture==TEXTURES) {
                            plosa->texture=0;
                        }
                        break;
                    case CP_PENCIL:
                        plosa->bender=!plosa->bender;
                        break;
                    case CP_SAVE:
                        dosave();
                        break;
                    }
                } else if(draw_flagconfig_enabled) {
                    if(plosa->configpos >= THEMES) {
                        plosa->configpos = 0;
                    }
                    //printf("flag selected: %d\n",plosa->configpos);
                    if((plosa->theme==0 && plosa->configpos>0)||(plosa->configpos==0 && plosa->theme>0)) {
                        gdeg_fine += 180;
                        edeg_fine += 180;
                    }
                    plosa->theme = plosa->configpos;

                    draw_gfx_enabled=true;
                    draw_text_enabled=true;
                    draw_flagconfig_enabled=false;
                    draw_flagconfig_enabled=false;
                    hg_enabled = false;
                    cmode=CM_None;
                }
            } else if(cmode==CM_Changepos) {
                cmode=CM_Editpos;
                hgx = (int16_t)get_acc02f(acc[0],acc[1],1.0f); //acc[0];
                hgy = (int16_t)get_acc12f(acc[0],acc[1],1.0f); //acc[1];
                hg_enabled = true;
                //printf("hgxy: %d %d\n",hgx,hgy);
                tcw = false;
                tccw = false;
                colors[plosa->editpos]=changecol;
                if(plosa->editpos==EPOS_CONFIG) {
                    draw_gfx_enabled= false;
                    draw_text_enabled=false;
                    draw_config_enabled=true;
                    cmode=CM_Config;
                }
                if(plosa->editpos==EPOS_CENTER) {
                    draw_gfx_enabled= false;
                    draw_text_enabled=false;
                    draw_flagconfig_enabled=true;
                    cmode=CM_Config;
                }
            } else if(cmode==CM_Editpos) {
                cmode=CM_None;
                colors[plosa->editpos]=dcolors[plosa->editpos];
                rtc_set_datetime(&plosa->dt);
                if(!(plosa->dt.year%4)) {
                    last[2]=29;
                }
                else {
                    last[2]=28;
                }
                hg_enabled = false;

                if(draw_flagconfig_enabled) {
                    update_pos_matrix();
                    draw_gfx_enabled=true;
                    draw_text_enabled=true;
                    draw_flagconfig_enabled=false;
                }
            }
            fire=false;
        }
        if(cmode==CM_Changepos || cmode==CM_Editpos || cmode==CM_Config) {
            int16_t asx = get_acc02f(acc[0],acc[1],1.0f);
            int16_t asy = get_acc12f(acc[0],acc[1],1.0f);
            //int asx = (int)acc[0];
            asx-=hgx;
            asx>>1;
            asx<<1;
            if( asx > HOURGLASSBORDER || asx < -HOURGLASSBORDER ) {
                int16_t a = asx;
                if(a<0) {
                    a=-a;
                }
                hourglass_x -= a;
                a>>=2;
                if( hourglass_x <=0 ) {
                    hourglass_x=HOURGLASS;
                    if(asx>0) {
                        dir_y=D_MINUS;
                        tcw=true;
                    }
                    if(a==0) {
                        dir_y=D_NONE;
                    }
                    if(asx<0) {
                        dir_y=D_PLUS;
                        tccw=true;
                    }
                }
            }
            //int asy = (int)acc[1];
            asy-=hgy;
            asy>>1;
            asy<<1;
            if( asy > HOURGLASSBORDER || asy < -HOURGLASSBORDER ) {
                int16_t a = asy;
                if(a<0) {
                    a=-a;
                }
                hourglass_y -= a;
                a>>=2;
                if( hourglass_y <=0 ) {
                    hourglass_y=HOURGLASS;
                    if(asy>0) {
                        dir_x=D_PLUS;
                        tcw=true;
                    }
                    if(a==0) {
                        dir_x=D_NONE;
                    }
                    if(asy<0) {
                        dir_x=D_MINUS;
                        tccw=true;
                    }
                }
            }
        }

        if(cmode==CM_Changepos) {
            if(NO_POS_MODE) {
                if(dir_x==D_PLUS) {
                    if(pos_matrix_x<positions[plosa->theme]->dim_x-1)++pos_matrix_x;
                }
                if(dir_x==D_MINUS) {
                    if(pos_matrix_x>0)--pos_matrix_x;
                }
                if(dir_y==D_PLUS) {
                    if(pos_matrix_y<positions[plosa->theme]->dim_y-1)++pos_matrix_y;
                }
                if(dir_y==D_MINUS) {
                    if(pos_matrix_y>0)--pos_matrix_y;
                }
                colors[plosa->editpos]=dcolors[plosa->editpos];
                plosa->editpos=positions[plosa->theme]->pos[pos_matrix_y*(positions[plosa->theme]->dim_x)+pos_matrix_x];
                dir_x = D_NONE;
                dir_y = D_NONE;
            } else {
                // change editposition (l/r or u/d) [ur+](tcw) [dl-](tccw)
                if(tcw) {
                    colors[plosa->editpos]=dcol;
                    if(plosa->editpos==EDITPOSITIONS) {
                        plosa->editpos=0;
                    }
                    else {
                        ++plosa->editpos;
                    }
                    colors[plosa->editpos]=editcol;
                    tcw=false;
                } else if(tccw) {
                    colors[plosa->editpos]=dcol;
                    if(plosa->editpos==0) {
                        plosa->editpos=EDITPOSITIONS;
                    }
                    else {
                        --plosa->editpos;
                    }
                    colors[plosa->editpos]=editcol;
                    tccw=false;
                }
            }
        }

        if(cmode==CM_Editpos || cmode==CM_Config) {
            bool set=false;
            if(tcw) {
                colors[plosa->editpos]=changecol;
                switch(plosa->editpos) {
                case 0:
                    (plosa->dt.dotw==6)?plosa->dt.dotw=0:plosa->dt.dotw++;
                    break;
                case 1:
                    (plosa->dt.day==last[plosa->dt.month])?plosa->dt.day=1:plosa->dt.day++;
                    break;
                case 2:
                    (plosa->dt.month==12)?plosa->dt.month=1:plosa->dt.month++;
                    break;
                case 3:
                    (plosa->dt.year==2099)?plosa->dt.year=2022:plosa->dt.year++;
                    break;
                case 4:
                    (plosa->dt.hour==23)?plosa->dt.hour=0:plosa->dt.hour++;
                    break;
                case 5:
                    (plosa->dt.min==59)?plosa->dt.min=0:plosa->dt.min++;
                    break;
                case 6:
                    (plosa->dt.sec==59)?plosa->dt.sec=0:plosa->dt.sec++;
                    break;
                case EPOS_CONFIG:
                    break;
                }
                tcw=false;
            }
            if(tccw) {
                colors[plosa->editpos]=changecol;
                switch(plosa->editpos) {
                case 0:
                    (plosa->dt.dotw==0)?plosa->dt.dotw=6:plosa->dt.dotw--;
                    break;
                case 1:
                    (plosa->dt.day==1)?plosa->dt.day=last[plosa->dt.month]:plosa->dt.day--;
                    break;
                case 2:
                    (plosa->dt.month==1)?plosa->dt.month=12:plosa->dt.month--;
                    break;
                case 3:
                    (plosa->dt.year==2099)?plosa->dt.year=2022:plosa->dt.year--;
                    break;
                case 4:
                    (plosa->dt.hour==0)?plosa->dt.hour=23:plosa->dt.hour--;
                    break;
                case 5:
                    (plosa->dt.min==0)?plosa->dt.min=59:plosa->dt.min--;
                    break;
                case 6:
                    (plosa->dt.sec==0)?plosa->dt.sec=59:plosa->dt.sec--;
                    break;
                case EPOS_CONFIG:
                    break;
                }
                tccw=false;
                //set=true;
            }
            if(set) {
                rtc_set_datetime(&plosa->dt);
                if(!(plosa->dt.year%4)) {
                    last[2]=29;
                }
                else {
                    last[2]=28;
                }
            }
        }
        if(cmode==CM_Editpos || cmode==CM_Changepos || cmode==CM_Config) {
            uint16_t cmode_color = GREEN;
            if(cmode==CM_Changepos) {
                blink_counter++;
                if(blink_counter==5) {
                    bmode=!bmode;
                    blink_counter=0;
                }
                cmode_color = blinker[bmode];
            }
            if(plosa->editpos==0) {
                if(plosa->theme==0) {
                    lcd_frame(POS_CNDOW_X-6,POS_CNDOW_Y,POS_CNDOW_X+CNFONT.w+3,POS_CNDOW_Y+CNFONT.h*3+1,cmode_color,3);
                } else {
                    fx_circle(tpos[plosa->editpos].x+18,tpos[plosa->editpos].y+8,25,cmode_color,3,xold,yold);
                }
            } else if(plosa->editpos==3) {
                fx_circle(tpos[plosa->editpos].x+12,tpos[plosa->editpos].y+5,28,cmode_color,3,xold,yold);
            } else if(plosa->editpos==EPOS_CONFIG) {
                if(cmode==CM_Changepos) {
                    draw_doimage(*adoi_config[plosa->theme]);
                }
                int16_t x= (*adoi_config[plosa->theme])->vpos.x+15;
                int16_t y= (*adoi_config[plosa->theme])->vpos.y+16;
                fx_circle(x,y,22,cmode_color,3,xold,yold);

                Vec2 dp1 = texsize[plosa->texture];
                draw_clock_hands();

            } else if(plosa->editpos==EPOS_CENTER) {
                if(cmode==CM_Config) {
                    int16_t x= (*adoi_config[plosa->theme])->vpos.x+15;
                    int16_t y= (*adoi_config[plosa->theme])->vpos.y+16;
                    fx_circle(x,y,22,cmode_color,3,xold,yold);
                    Vec2 dp1 = texsize[plosa->texture];
                    draw_clock_hands();
                } else {
                    fx_circle(tpos[plosa->editpos].x,tpos[plosa->editpos].y,19,cmode_color,3,xold,yold);
                }
            } else {
                fx_circle(tpos[plosa->editpos].x+12,tpos[plosa->editpos].y+5,20,cmode_color,3,xold,yold);
            }
        }

        if(!theme_bg_dynamic_mode) {
            if(!plosa->highpointer||cmode==CM_Editpos) {
                draw_gfx();
                draw_text();
            } else {
                draw_text();
                draw_gfx();
            }
        }
        if(draw_config_enabled) {
            if(plosa->spin!=0) {
                flagdeg = gdeg(flagdeg+plosa->spin);
            }
            edeg_fine = draw_getdeg(edeg_fine);
            int16_t magnet = draw_circmenu(edeg_fine, 8, config_images);
            if(magnet != 0) {
                int16_t MAG = 10;
                if(magnet < MAG && magnet >-MAG) {
                    if(magnet/2 == 0) {
                        edeg_fine-= magnet;
                    }
                    else {
                        edeg_fine -= (magnet)/2;
                    }
                } else {
                    edeg_fine -= magnet/8;
                }
            }
        }
#define magx 35
#define magy 90
#define mags 20
#define magy2 magy+mags+20
#define magx2 magx-10
#define magf 2
        //lcd_magnify(magx,magy,mags,magx2,magy2,magf);
        //lcd_frame(magx,magy,magx+mags,magy+mags,RED,1);
        //lcd_frame(magx2,magy2,magx2+mags*magf,magy2+mags*magf,RED,1);

        if(false && draw_flagconfig_enabled) {
            if(plosa->spin!=0) {
                flagdeg = gdeg(flagdeg+plosa->spin);
            }
            gdeg_fine = draw_getdeg(gdeg_fine);
            int16_t magnet = draw_circmenu(gdeg_fine, THEMES, flags);
            if(magnet != 0) {
                //printf("magnet = %d\n",magnet);
                int16_t MAG = 15;
                if(magnet < MAG && magnet >-MAG) {
                    if(magnet/2 == 0) {
                        gdeg_fine-= magnet;
                    }
                    else {
                        gdeg_fine -= (magnet)/2;
                    }
                } else {
                    gdeg_fine -= magnet/8;
                }
            }
        }

        lcd_display(b0);

        if(SHELL_ENABLED) {
            shell();
        }
        //Vec2 vs = {120,120};
        //Vec2 ve = {32,32};
        //lcd_blit_deg(vs,ve,deg,usa32);
        plosa->save_crc=crc(&plosa->theme,LOSASIZE);
        //uint32_t atime = time_us_32();
        //uint32_t wtime = ((atime-last_wait)/100000);
        //last_wait = atime;
        ////printf("wt= %d [%d]\n",wtime,FRAME_DELAY-wtime);
        //if(wtime>=frame_delay){wtime=frame_delay-1;}
        //sleep_ms(frame_delay-wtime);

    }
    return 0;
}
