#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/init_task.h>
#include <linux/timer.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Krivozubov Vlad");

static struct timer_list timer;

// process_time_data[] - времена(в сек), которое использовал каждый процесс за период отчетности (допустим час)
// mouse_data[] - события мыши - приходит новые события с интервалом 10 сек
// keyboard_data[] - нажатые клавиши - приходят новые нажатия с интервалом 10 сек

// раз в час запускаем коллбэк второго таймера - компануем и отправляем отчет через сокеты

void timer_callback(struct timer_list  *timer)
{
    printk("timer triggered!\n");
    mod_timer(timer, jiffies + msecs_to_jiffies(1000));
}

// тут будет большой бизнес логика с обработкой процессов и компановкой часового отчета

static int __init md_init(void) 
{
    timer_setup(&timer, timer_callback, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

	printk("LAB3: module is loaded.\n");
	struct task_struct *task = &init_task;
 	do 
	{
 		printk("LAB3:---%s-%d, parent %s-%d", task->comm,
 		task->pid, task->parent->comm, task->parent->pid);
 	} while ((task = next_task(task)) != &init_task);
 	printk("LAB3:---%s-%d, parent %s-%d", current->comm,
 	current->pid, current->parent->comm, current->parent->pid);
 	return 0;
}

static void __exit md_exit(void) 
{
    del_timer(&timer);
	printk("LAB3: module is unloaded.\n");
}

module_init(md_init);
module_exit(md_exit);