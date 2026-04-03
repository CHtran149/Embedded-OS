#include <linux/module.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h> // Thêm thư viện này để dùng mdelay/ssleep

#define DEVICE_NAME "pzem_sensor"
#define CLASS_NAME  "pzem"

static int major;
static struct class* pzem_class  = NULL;
static struct device* pzem_device = NULL;
static struct serdev_device *pzem_serdev = NULL;

static char message[128] = "No data yet\n";

static uint16_t modbus_crc(const uint8_t *buf, uint8_t len) {
    uint16_t crc = 0xFFFF;
    uint8_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= buf[i];
        for (j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

// 1. Hàm nhận dữ liệu từ UART (Chạy trong ngữ cảnh ngắt/softirq)
static int pzem_receive_buf(struct serdev_device *serdev, const unsigned char *buf, size_t len) {
    uint16_t crc_calc, crc_recv;
    uint16_t v_val, f_val, pf_val;
    uint32_t i_val, p_val;

    // Chỉ xử lý nếu nhận đủ frame phản hồi 25 bytes của PZEM v3
    if (len >= 25 && buf[1] == 0x04) {
        
        // Kiểm tra CRC để đảm bảo dữ liệu không bị nhiễu trên đường dây
        crc_calc = modbus_crc(buf, 23);
        crc_recv = buf[23] | (buf[24] << 8);

        if (crc_calc != crc_recv) {
            printk(KERN_ERR "PZEM: Sai ma CRC! Calc: %04x, Recv: %04x\n", crc_calc, crc_recv);
            return len;
        }

        // GIẢI MÃ DỮ LIỆU (Áp dụng đúng thứ tự byte từ thư viện ESP32)
        
        // Voltage (2 byte): [3] High, [4] Low -> chia 10
        v_val = (buf[3] << 8) | buf[4];

        // Current (4 byte): Theo ESP32 là (buf[7]<<24 | buf[8]<<16 | buf[5]<<8 | buf[6]) -> chia 1000
        i_val = ((uint32_t)buf[7] << 24) | ((uint32_t)buf[8] << 16) | 
                ((uint32_t)buf[5] << 8)  | buf[6];

        // Power (4 byte): (buf[11]<<24 | buf[12]<<16 | buf[9]<<8 | buf[10]) -> chia 10
        p_val = ((uint32_t)buf[11] << 24) | ((uint32_t)buf[12] << 16) | 
                ((uint32_t)buf[9] << 8)  | buf[10];

        // Frequency (2 byte): [17] High, [18] Low -> chia 10
        f_val = (buf[17] << 8) | buf[18];

        // Power Factor (2 byte): [19] High, [20] Low -> chia 100
        pf_val = (buf[19] << 8) | buf[20];

        // Ghi kết quả vào biến message để App đọc
        snprintf(message, sizeof(message), 
                 "U: %u.%uV | I: %u.%uA | P: %u.%uW | F: %u.%uHz | PF: %u.%u\n", 
                 v_val / 10, v_val % 10, 
                 i_val / 1000, i_val % 1000,
                 p_val / 10, p_val % 10,
                 f_val / 10, f_val % 10,
                 pf_val / 100, pf_val % 100);
        
        //printk(KERN_INFO "PZEM: Update successful\n");
    }
    return len;
}

static const struct serdev_device_ops pzem_ops = {
    .receive_buf = pzem_receive_buf,
};

// 2. Hàm đọc từ User-space
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    size_t datalen;
    // Khai báo mảng cmd ở ĐẦU HÀM để tránh lỗi ISO C90
    unsigned char cmd[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D};
    //printk(KERN_INFO "PZEM: Bat dau ham dev_read...\n");

    if (*offset > 0) return 0;

    if (pzem_serdev == NULL) {
        printk(KERN_ERR "PZEM: Loi! pzem_serdev dang bi NULL\n");
        return -EIO;
    }

    datalen = strlen(message);

    //printk(KERN_INFO "PZEM: Dang gui lenh doc den cam bien...\n");
    
    if (pzem_serdev) {
        serdev_device_write_buf(pzem_serdev, cmd, sizeof(cmd));

        //printk(KERN_INFO "PZEM: Da gui lenh Modbus, dang doi phan hoi...\n");

        msleep(300); // Tăng lên 300ms cho chắc chắn
    } 

    error_count = copy_to_user(buffer, message, datalen);
    if (error_count == 0) {
        *offset = datalen;
        return datalen;
    } else return -EFAULT;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

// 3. Hàm Probe
static int pzem_probe(struct serdev_device *serdev) {
    int status;
    pzem_serdev = serdev;

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) return major;

    pzem_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(pzem_class)) {
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(pzem_class);
    }

    pzem_device = device_create(pzem_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(pzem_device)) {
        class_destroy(pzem_class);
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(pzem_device);
    }

    serdev_device_set_client_ops(serdev, &pzem_ops);
    status = serdev_device_open(serdev);
    if (status) {
        device_destroy(pzem_class, MKDEV(major, 0));
        class_destroy(pzem_class);
        unregister_chrdev(major, DEVICE_NAME);
        return status;
    }

    serdev_device_set_baudrate(serdev, 9600);
    serdev_device_set_flow_control(serdev, false);

    //printk(KERN_INFO "PZEM Driver: Probe successful! /dev/%s created\n", DEVICE_NAME);

    msleep(100);
    unsigned char test_cmd[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D};
    //printk(KERN_INFO "PZEM: Dang gui lenh test ngay khi Probe...\n");
    serdev_device_write_buf(serdev, test_cmd, sizeof(test_cmd));

    return 0;
}

// 4. Hàm Remove sạch sẽ
static void pzem_remove(struct serdev_device *serdev) {
    serdev_device_close(serdev);
    if (pzem_class) {
        device_destroy(pzem_class, MKDEV(major, 0));
        class_destroy(pzem_class);
    }
    if (major > 0) {
        unregister_chrdev(major, DEVICE_NAME);
    }
    printk(KERN_INFO "PZEM Driver: Removed cleanly\n");
}

static const struct of_device_id pzem_ids[] = {
    { .compatible = "my-pzem,pzem004t", },
    { }
};
MODULE_DEVICE_TABLE(of, pzem_ids);

static struct serdev_device_driver pzem_driver = {
    .probe = pzem_probe,
    .remove = pzem_remove,
    .driver = {
        .name = "pzem-driver",
        .of_match_table = pzem_ids,
    },
};

module_serdev_device_driver(pzem_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chien");
MODULE_DESCRIPTION("Driver for PZEM-004T using Serdev");
