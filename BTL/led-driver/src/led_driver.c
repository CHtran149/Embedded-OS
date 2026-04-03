#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/device.h> // Cần thiết cho class và device

#define DEVICE_NAME "led_test"
#define CLASS_NAME  "led_class"
#define GPIO_LED    60

static int major_number;
static struct class* led_class  = NULL;
static struct device* led_device = NULL;

static ssize_t dev_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos) {
    char val;
    if (copy_from_user(&val, user_buffer, 1)) return -EFAULT;
    
    if (val == '1') {
        gpio_set_value(GPIO_LED, 1);
        //printk(KERN_INFO "LED: ON\n");
    } else if (val == '0') {
        gpio_set_value(GPIO_LED, 0);
        //printk(KERN_INFO "LED: OFF\n");
    }
    return count;
}

static struct file_operations fops = {
    .write = dev_write,
};

static int __init led_driver_init(void) {
    // 1. Đăng ký ký tự thiết bị
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) return major_number;

    // 2. Tạo lớp thiết bị (Device Class)
    led_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(led_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(led_class);
    }

    // 3. Tự động tạo node thiết bị trong /dev/
    led_device = device_create(led_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        class_destroy(led_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(led_device);
    }

    // 4. Thiết lập GPIO
    if (gpio_is_valid(GPIO_LED)) {
        gpio_request(GPIO_LED, "led_gpio");
        gpio_direction_output(GPIO_LED, 0);
    }

    printk(KERN_INFO "LED Driver Loaded: /dev/%s created\n", DEVICE_NAME);
    return 0;
}

static void __exit led_driver_exit(void) {
    gpio_set_value(GPIO_LED, 0);
    gpio_free(GPIO_LED);
    device_destroy(led_class, MKDEV(major_number, 0));
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "LED Driver Unloaded\n");
}

module_init(led_driver_init);
module_exit(led_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChienTran");
