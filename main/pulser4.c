/* Hardware timer generate sync pulse
 * Params in NVS
*/

#include <stdio.h>
#include <string.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#include "driver/gpio.h"

#include "duart.h"

//#define BLINK_GPIO 18
#define BLINK_GPIO 22
#define BLONK_GPIO 23

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

#define TIMER_INTERVAL0_SEC   (0.1) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (0.0001)   // sample test interval for the second timer
//#define TIMER_INTERVAL1_SEC   (0.1)   // sample test interval for the second timer

//#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer
//#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

#define GPIO_INPUT_IO_0     21
#define GPIO_INPUT_IO_1     5
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_OUTPUT_IO_0    22
#define GPIO_OUTPUT_IO_1    23
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

#define TMRC_TOP        180
//--
#define DELAY_MS	10
#define LONG_DELAY_MS	8000

//void blink();
void waitever();
void app_iosup();

static xQueueHandle gpio_evt_queue = NULL;

unsigned int rang1[] = {0, 1500, -1};
unsigned int rang2[] = {0, 100, 101, 1500, -1};
unsigned int rang3[] = {0, 15, 75, 100, 101, 500, -1};
unsigned int rang4[] = {0, 100, 101, 120, 160, 500, -1};
unsigned int rang5[] = {0, 100, 101, 120, 160, 500, 501, 502, 503, 504, 505, 506, 507, -1};
unsigned int rang6[] = {0, 100, 101, 120, 160, 600, 601, 602, 603, 604, 605, 606, 607, -1};

static unsigned int *sigim[] =
{
    rang1, rang2, rang3, rang4, (unsigned int *)-1
}
;

unsigned int *sigcur;
unsigned int pulcode = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static timer_config_t config;
    timer_get_config(0, 1, &config);
    sigim[3] = rang4;
    pulcode = 0;
    sigcur = (unsigned int *)xvec; //xline? xvec?
/* predefined set of pulses repeated by changing pulcode
        sigcur = sigim[pulcode];
        pulcode = (pulcode +1) % 3;
*/
    gpio_set_level(BLONK_GPIO, 0);
    if(config.counter_en == TIMER_PAUSE) {
        gpio_set_level(BLINK_GPIO, 1);
       	timer_start(TIMER_GROUP_0, 1); //0?
    }
}

static unsigned int siginit=0;

static void gpio_task_inp(void* arg)
{
    int callcnt = 0;
    uint32_t io_num;
    for(;;) {
        //no receive expected
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            callcnt++;
            siginit=0;
            gpio_set_level(BLONK_GPIO, 0);
            timer_start(TIMER_GROUP_0, 1); //0?
        }
    }
}

void waitever()
{
   static timer_config_t config;
   while(1)
   {
        vTaskDelay(LONG_DELAY_MS / portTICK_RATE_MS);
        timer_get_config(0, 1, &config);
	printf("ctr en %d\n", config.counter_en);
   }
}
//--

typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

//xQueueHandle timer_queue;

static void inline print_timer_counter(uint64_t counter_value)
{
int64_t tt = esp_timer_get_time();
    printf("esp time: 0x%08x%08x\n", (uint32_t) (tt >> 32), (uint32_t) (tt));
    printf("Time    : %.8f s\n", (double) counter_value / TIMER_SCALE);
}

static char* disp_vec(char *out, int vec[])
{
    //char *o = (char *) malloc(70);;
    //char *o = out;
    for(int i=0; vec[i] != (unsigned int)-1; i++) {
	printf("%d:%d,", i, vec[i]);
        //asprintf(&t, "%d,", vec[i]);
        //strcat(o, t);
    }
    printf("\n");
    return("\n");
}

void IRAM_ATTR timer_group0_isr(void *para)
{
    static int flip=0;
    static unsigned int tmrc = 0;
    //static unsigned int rng[] = {0, 100, 101, 1500};  //mid pulse mark
    //static unsigned int rng[] = {0, 15, 75, 100, 101}; //pos side pulse
    //static unsigned int rng[] = {0, 100, 101, 120, 160}; //neg side pulse
    //static unsigned int rng[] = {0, 15, 75, 100, 101, 120, 160}; //both sides pulse
    static unsigned int *rng;
rng = sigcur;

    static int sig = 0;
    static int ax = 0;
    //int r=0;

    int timer_idx = (int) para;

    if(siginit == 0) {
        siginit = 1;
	sig = 0;
        gpio_set_level(BLONK_GPIO, 0);
    }
    TIMERG0.hw_timer[timer_idx].update = 1;

    TIMERG0.int_clr_timers.t1 = 1;

    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    //xQueueSendFromISR(timer_queue, &evt, NULL);
    gpio_set_level(BLINK_GPIO, flip);
    flip ^= 1;
    tmrc++;

/*
for i in range(69):
...   if i>=rng[ax]:
...     print 'sig:', i,'next:',rng[ax+1]
...     ax = ax+1
*/
    if (tmrc >= rng[ax])
    {
         gpio_set_level(BLONK_GPIO, sig );
	 if(rng[ax+1] != (-1)) {
             ax += 1;
             sig ^= 1;
	 }
    }
    if(tmrc > TMRC_TOP) {
        timer_pause(TIMER_GROUP_0, timer_idx);
        tmrc = 1;
        gpio_set_level(BLINK_GPIO, 0);
	sig = 0;
        gpio_set_level(BLONK_GPIO, 0);
        ax = 0;
    }
        
}

static void example_tg0_timer_init(int timer_idx, 
    bool auto_reload, double timer_interval_sec)
{
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
}

void parsevec(char **svec, int *vec, int maxsvec)
{
    int vi = 0;
    int val = 0;
    char g = '9';

    printf("svec[%d]:", maxsvec);
    for(int i=0; i<maxsvec; i++) {
        printf("[%d]:<%s>^", i, svec[i]);

	if(strcmp(svec[i], "-1") == 0) {
	    printf("  grp end %c:-1\n", g);
	    continue;
	} else if(strcmp(svec[i], "-9") == 0) {
	    break;
	} else if(strcmp(svec[i], "E") == 0) {
	    break;
	} else if(strncmp(svec[i], "PM", 2) == 0) {
	    vi = 0;
	    g = svec[i][2];
            printf("  grp g=<%c>\n", g);
	    continue;
	} else if(strchr("-0123456789", svec[i][0])!=0) {
            printf("D<%s>\n", svec[i]);
            val = atoi(svec[i]);
            vec[vi++] = val;
	}
    }
    printf("\n");
    vec[vi] = -1;

    printf("vec array: ");
    for(int i=0; vec[i]>=0; i++)
        printf("vec[%d]=%d,", i, vec[i]);
    printf("\n");
}

void tstnvs()
{
    printf("--- test named NVS\n");
    char mem[60];
    esp_err_t err;

    err = get_named_str(mem, "pm0", 58);
    if (err != ESP_OK) { printf("get_named err0=%d\n", err);}
    printf("read %s:<%s>\n\n", "pm0", mem);
}

void loadtrain(int cnt, char **names)
{
    printf("@ loadtrain <%s>:", names[0]);
    printf("@ loadtrain [%d]\n", cnt);
    for(int k=0; k<cnt; k++){printf("%s,", names[k]);}
    printf("@ \nloadtrain\n");
}

void app_main()
{
        int vec2[30];

	printf("pulser4 - generate synced timed pulse\nuse internal NVS list\n");
	nvs_starter();
	tstnvs();

	uart_task(NULL);
        printf("after uart xline:<%s>\n", xline);

	char *o = (char *) malloc(90);
	strncpy(o, xline, 90);
        if(o[0] == (char)0) {
            printf("nothing in xline! exit.\n");
	    return;
        }

	char **arsplit;
	arsplit = malloc(sizeof(char *) * 20);
	int argim = getArgs((char *)(o), arsplit, 20);
        printf("get xline(%s):%d name <%s>\n", xline, argim, arsplit[0]);

        loadnmstr(vec2, arsplit[0], 14);
        loadnmstr(xvec, arsplit[0], 14); //global xvec

	if(strncmp(arsplit[0], "tr", 2) == 0) {
	    loadtrain(argim, arsplit);
	} else {
            printf("..._ loaded signal<%s>:", arsplit[0]);
            disp_vec(o, vec2);
            printf("_...\n");
        }
	sigcur = (unsigned int *)vec2; //! generate single pulse


	unsigned int *sug0 = malloc(sizeof(char *) * 50);
	static unsigned int *sugim[] = {rang5,  rang6, (unsigned int *)0, 0, 0}; //unused
sugim[2] = sug0;
sugim[3] = (unsigned int *) malloc(sizeof(char *) * 20);

printf("(unused) sugim[0]:%x\n", sugim[0][0]);
    example_tg0_timer_init(TIMER_1, TEST_WITH_RELOAD,    TIMER_INTERVAL1_SEC);
    //xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
    printf("timer scale:%d\n", TIMER_SCALE);
    printf("interval0:%f\n", TIMER_INTERVAL0_SEC);
    printf("interval1:%f\n", TIMER_INTERVAL1_SEC);

    app_iosup();

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    printf("input waiting on GPIO%llx\n", GPIO_INPUT_PIN_SEL);

}

void app_iosup()
{
    printf("HWSUP test sync speed\n");

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_POSEDGE);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_inp, "gpio_task_inp", 2048, NULL, 10, NULL);
    printf("input gpio_task_inp running, waiting for %x\n", (unsigned)GPIO_INPUT_PIN_SEL);
    printf("BLONK level %x\n", gpio_get_level( BLONK_GPIO ));
}

/*
static void dumb_task(void *arg)
{
    waitever();
}
*/
