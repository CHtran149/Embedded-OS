#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

#define GPIO_BUTTON 67 
#define DEVICE_NAME "bbb_button"

static struct input_dev *button_dev;
static int irq_num;

static unsigned long last_interrupt_time = 0;

static irqreturn_t button_isr(int irq, void *dev_id) {
    int pin_value = gpio_get_value(GPIO_BUTTON);
    static unsigned long last_jiffies = 0;

    // Chỉ cho phép xử lý nếu cách lần cuối > 50ms (đủ để lọc nhiễu cơ khí)
    if (time_before(jiffies, last_jiffies + msecs_to_jiffies(50))) {
        return IRQ_HANDLED;
    }

    // Báo cáo trạng thái: Nhấn (1) khi pin=0, Thả (0) khi pin=1
    input_report_key(button_dev, KEY_A, !pin_value);
    input_sync(button_dev);
    
    last_jiffies = jiffies;
    
    // In log để debug chắc chắn trong dmesg
    // printk(KERN_INFO "Button: GPIO=%d, Value=%d\n", GPIO_BUTTON, !pin_value);
    
    return IRQ_HANDLED;
}

static int __init button_init(void) {
    int ret;

    button_dev = input_allocate_device();
    if (!button_dev) return -ENOMEM;

    button_dev->name = "BBB Button";
    button_dev->id.bustype = BUS_HOST; // Thêm để Udev nhận diện tốt hơn

    set_bit(EV_KEY, button_dev->evbit);
    set_bit(KEY_A, button_dev->keybit);

    ret = input_register_device(button_dev);
    if (ret) {
        input_free_device(button_dev);
        return ret;
    }

    ret = gpio_request(GPIO_BUTTON, "button_gpio");
    if (ret) {
        input_unregister_device(button_dev);
        return ret;
    }
    
    gpio_direction_input(GPIO_BUTTON);
    irq_num = gpio_to_irq(GPIO_BUTTON);

    ret = request_irq(irq_num, button_isr, 
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 
                      "button_handler", NULL);
    if (ret) {
        gpio_free(GPIO_BUTTON);
        input_unregister_device(button_dev);
        return ret;
    }

    printk(KERN_INFO "Button Input Driver Loaded: Name='BBB Button'\n");
    return 0;
}

static void __exit button_exit(void) {
    free_irq(irq_num, NULL);
    gpio_free(GPIO_BUTTON);
    input_unregister_device(button_dev);
    printk(KERN_INFO "Button Input Driver Unloaded\n");
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChienTran");
