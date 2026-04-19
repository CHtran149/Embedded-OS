#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

// Định nghĩa 2 GPIO cho 2 nút (Chiến hãy kiểm tra chân thực tế trên board nhé)
#define GPIO_BUTTON_UP   67  // Nút Tăng (P8.11)
#define GPIO_BUTTON_DOWN 68  // Nút Giảm (P8.12) - Ví dụ chân cạnh bên
#define DEVICE_NAME "bbb_two_buttons"

static struct input_dev *buttons_dev;
static int irq_num_up, irq_num_down;

// Cấu trúc để quản lý riêng biệt từng nút
struct button_data {
    int gpio;
    int key_code;
    unsigned long last_jiffies;
    const char *label;
};

static struct button_data btn_up = {GPIO_BUTTON_UP, KEY_UP, 0, "btn_up"};
static struct button_data btn_down = {GPIO_BUTTON_DOWN, KEY_DOWN, 0, "btn_down"};

// ISR dùng chung cho cả 2 ngắt
static irqreturn_t buttons_isr(int irq, void *dev_id) {
    struct button_data *data = (struct button_data *)dev_id;
    int pin_value = gpio_get_value(data->gpio);

    // [1] Lọc nhiễu (Debounce) 50ms cho từng nút riêng biệt
    if (time_before(jiffies, data->last_jiffies + msecs_to_jiffies(50))) {
        return IRQ_HANDLED;
    }

    // [2] Báo cáo sự kiện: Nhấn (1) khi pin=0 (Active Low), Thả (0) khi pin=1
    input_report_key(buttons_dev, data->key_code, !pin_value);
    input_sync(buttons_dev);
    
    data->last_jiffies = jiffies;

    // printk(KERN_INFO "Button %s: Value=%d\n", data->label, !pin_value);
    
    return IRQ_HANDLED;
}

static int __init buttons_init(void) {
    int ret;
    
    btn_up.last_jiffies = jiffies - msecs_to_jiffies(50);
    btn_down.last_jiffies = jiffies - msecs_to_jiffies(50);

    // 1. Cấu hình Input Device
    buttons_dev = input_allocate_device();
    if (!buttons_dev) return -ENOMEM;

    buttons_dev->name = "BBB Two Buttons";
    buttons_dev->id.bustype = BUS_HOST;

    set_bit(EV_KEY, buttons_dev->evbit);
    set_bit(KEY_UP, buttons_dev->keybit);   // Phím Tăng
    set_bit(KEY_DOWN, buttons_dev->keybit); // Phím Giảm

    ret = input_register_device(buttons_dev);
    if (ret) {
        input_free_device(buttons_dev);
        return ret;
    }

    // 2. Cấu hình GPIO và IRQ cho nút UP
    gpio_request(btn_up.gpio, "btn_up_gpio");
    gpio_direction_input(btn_up.gpio);
    irq_num_up = gpio_to_irq(btn_up.gpio);
    request_irq(irq_num_up, buttons_isr, 
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 
                "btn_up_handler", &btn_up);

    // 3. Cấu hình GPIO và IRQ cho nút DOWN
    gpio_request(btn_down.gpio, "btn_down_gpio");
    gpio_direction_input(btn_down.gpio);
    irq_num_down = gpio_to_irq(btn_down.gpio);
    request_irq(irq_num_down, buttons_isr, 
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 
                "btn_down_handler", &btn_down);

    printk(KERN_INFO "Two Buttons Driver Loaded: UP=%d, DOWN=%d\n", btn_up.gpio, btn_down.gpio);
    return 0;
}

static void __exit buttons_exit(void) {
    free_irq(irq_num_up, &btn_up);
    free_irq(irq_num_down, &btn_down);
    gpio_free(btn_up.gpio);
    gpio_free(btn_down.gpio);
    input_unregister_device(buttons_dev);
    printk(KERN_INFO "Two Buttons Driver Unloaded\n");
}

module_init(buttons_init);
module_exit(buttons_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChienTran");
