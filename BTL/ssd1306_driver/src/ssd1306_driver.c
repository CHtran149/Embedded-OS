#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define DEVICE_NAME "oled_ssd1306"
#define CLASS_NAME  "ssd1306_class"

static struct i2c_client *oled_client = NULL;
static int major_number;
static struct class* oled_class  = NULL;
static struct device* oled_device = NULL;

/* 1. Hàm gửi Lệnh hoặc Dữ liệu qua I2C */
static int oled_write_reg(uint8_t is_data, uint8_t value) {
    /* SSD1306 I2C Protocol: Byte 1 là Control Byte (0x00: Command, 0x40: Data) */
    uint8_t buf[2] = {is_data ? 0x40 : 0x00, value};
    return i2c_master_send(oled_client, buf, 2);
}

/* 2. Chuỗi khởi tạo OLED (Init Sequence) */
static void ssd1306_init(void) {
    pr_info("SSD1306: Starting initialization sequence...\n");

    msleep(500);
    oled_write_reg(0, 0xAE);
    msleep(10);
    oled_write_reg(0, 0xD5); // Set Display Clock Divide Ratio
    oled_write_reg(0, 0x80);
    oled_write_reg(0, 0xA8); // Set Multiplex Ratio
    oled_write_reg(0, 0x3F);
    oled_write_reg(0, 0xD3); // Set Display Offset
    oled_write_reg(0, 0x00);
    oled_write_reg(0, 0x40); // Set Display Start Line
    oled_write_reg(0, 0x8D); // Charge Pump
    oled_write_reg(0, 0x14); // Enable Charge 
	
    msleep(10);
    oled_write_reg(0, 0x20); // Memory Addressing Mode
    oled_write_reg(0, 0x00); // Horizontal Mode
    oled_write_reg(0, 0xA1); // Set Segment Re-map (X-mirror)
    oled_write_reg(0, 0xC8); // Set COM Output Scan Direction (Y-mirror)
    oled_write_reg(0, 0xDA); // Set COM Pins Hardware Configuration
    oled_write_reg(0, 0x12);
    oled_write_reg(0, 0x81); // Set Contrast
    oled_write_reg(0, 0xCF);
    oled_write_reg(0, 0xD9); // Set Pre-charge Period
    oled_write_reg(0, 0xF1);
    oled_write_reg(0, 0xDB); // Set VCOMH Deselect Level
    oled_write_reg(0, 0x40);
    oled_write_reg(0, 0xA4); // Entire Display ON
    oled_write_reg(0, 0xA6); // Normal Display
    oled_write_reg(0, 0xAF); // Display ON
	
    msleep(50);
    oled_write_reg(0, 0xA4);
    pr_info("SSD1306: Initialization complete.\n");
}

static int oled_write_block(const uint8_t *data, size_t len) {
    uint8_t *buf;
    int ret;

    // Cấp phát bộ nhớ tạm (Dữ liệu + 1 byte Control)
    buf = kmalloc(len + 1, GFP_KERNEL);
    if (!buf) return -ENOMEM;

    buf[0] = 0x40; // Control Byte: Dữ liệu liên tục (Stream)
    memcpy(&buf[1], data, len);

    // Gửi toàn bộ gói tin qua I2C trong 1 lần duy nhất
    ret = i2c_master_send(oled_client, buf, len + 1);
    
    kfree(buf);
    return ret;
}

/* 3. Giao diện ghi dữ liệu từ User-space */
static ssize_t oled_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    uint8_t *kbuf;
    
    // SSD1306 chỉ có tối đa 1024 byte RAM (128x64/8)
    if (count > 1024) count = 1024;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    // Gửi dữ liệu khối xuống màn hình
    oled_write_block(kbuf, count);

    kfree(kbuf);
    return count;
}


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = oled_dev_write,
};

/* 4. Hàm Probe: Chạy khi Device Tree khớp với compatible string */
static int ssd1306_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    pr_info("SSD1306: Probing device at address 0x%x\n", client->addr);
    
    oled_client = client;

    // A. Đăng ký Character Device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("SSD1306: Failed to register major number\n");
        return major_number;
    }

    // B. Tạo Class (xuất hiện trong /sys/class/)
    oled_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(oled_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(oled_class);
    }

    // C. Tạo Device node (tự động tạo /dev/oled_ssd1306)
    oled_device = device_create(oled_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(oled_device)) {
        class_destroy(oled_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(oled_device);
    }

    // D. Gửi chuỗi khởi tạo lên OLED
    ssd1306_init();

    pr_info("SSD1306: Driver loaded successfully. Device file: /dev/%s\n", DEVICE_NAME);
    return 0;
}

/* 5. Hàm Remove: Dọn dẹp khi gỡ driver */
static int ssd1306_remove(struct i2c_client *client) {
    device_destroy(oled_class, MKDEV(major_number, 0));
    class_unregister(oled_class);
    class_destroy(oled_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("SSD1306: Driver unloaded.\n");
    return 0;
}

/* Khai báo tương thích với Device Tree */
static const struct of_device_id ssd1306_of_match[] = {
    { .compatible = "custom,ssd1306", },
    { },
};
MODULE_DEVICE_TABLE(of, ssd1306_of_match);

static struct i2c_driver ssd1306_i2c_driver = {
    .driver = {
        .name = "ssd1306_oled",
        .of_match_table = ssd1306_of_match,
    },
    .probe = ssd1306_probe,
    .remove = ssd1306_remove,
};

module_i2c_driver(ssd1306_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chien");
MODULE_DESCRIPTION("I2C Driver for SSD1306 OLED via Buildroot");
