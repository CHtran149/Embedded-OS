# TUẦN 7: Xây dựng Driver giao tiếp phần cứng cơ bản

## Bài 1: Hoàn thiên 1 Driver có đủ các hàm cơ bản

### Cấu trúc thư mục Driver
```
package/my_cdev/
├── Config.in
├── my_cdev.mk
└── src/
    ├── my_cdev.c
    └── Makefile

```

### File mã nguồn Driver
```
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "my_char_dev"
#define CLASS_NAME  "my_dev_class"

static int majorNumber;
static struct class* myClass  = NULL;
static struct device* myDevice = NULL;
static char kernel_buffer[1024];

// Hàm Open
static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "MyCharDev: Thiet bi da duoc mo\n");
    return 0;
}

// Hàm Read (copy_to_user)
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    size_t datalen = strlen(kernel_buffer);
    
    if (*offset >= datalen) return 0; // Het du lieu

    error_count = copy_to_user(buffer, kernel_buffer, datalen);

    if (error_count == 0) {
        printk(KERN_INFO "MyCharDev: Da gui du lieu cho User Space\n");
        *offset += datalen;
        return datalen;
    } else {
        printk(KERN_INFO "MyCharDev: Loi khi gui du lieu\n");
        return -EFAULT;
    }
}

// Hàm Write (copy_from_user)
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    memset(kernel_buffer, 0, sizeof(kernel_buffer));
    if (len > 1023) len = 1023;

    if (copy_from_user(kernel_buffer, buffer, len) != 0) {
        return -EFAULT;
    }
    printk(KERN_INFO "MyCharDev: Da nhan tu User Space: %s\n", kernel_buffer);
    return len;
}

// Hàm Release
static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "MyCharDev: Thiet bi da dong\n");
    return 0;
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Init Driver
static int __init mycdev_init(void) {
    // 1. Cap phat Major number dong
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "MyCharDev: Khong the dang ky Major number\n");
        return majorNumber;
    }

    // 2. Tao Class
    myClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(myClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(myClass);
    }
    
    // 3. Tao Device file (/dev/my_char_dev)
    myDevice = device_create(myClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(myDevice)) {
        class_destroy(myClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(myDevice);
    }
    
    printk(KERN_INFO "MyCharDev: Driver nap thanh cong voi Major %d\n", majorNumber);
    return 0;
}

// Exit Driver
static void __exit mycdev_exit(void) {
    device_destroy(myClass, MKDEV(majorNumber, 0));
    class_unregister(myClass);
    class_destroy(myClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "MyCharDev: Tam biet!\n");
}

module_init(mycdev_init);
module_exit(mycdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChienTran");
MODULE_DESCRIPTION("Character Driver co ban cho BeagleBone Black");
```

### File Makefile
```
obj-m += my_cdev.o

all:
	$(MAKE) -C $(LINUX_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(LINUX_DIR) M=$(PWD) clean
```

### File Config Buildroot
```
config BR2_PACKAGE_MY_CDEV
    bool "my_cdev"
    depends on BR2_LINUX_KERNEL
    help
      Module Driver Character Device co ban.
      Tu dong tao device node tai /dev/my_char_dev.
```
### File Make Buildroot
```
MY_CDEV_VERSION = 1.0
MY_CDEV_SITE = $(TOPDIR)/package/my_cdev/src
MY_CDEV_SITE_METHOD = local

$(eval $(kernel-module))
$(eval $(generic-package))
```
### Thêm vào Config.in chính
``` source "package/my_cdev/Config.in"```

### Cập nhật cấu hình
``` make menuconfig``` tìm **Target packages** --> **my_cdev**

### Biên dịch Driver
```
make my_cdev
make
```
### Nạp lại img mới vào thẻ
```
sudo dd if=output/images/sdcard.img of=/dev/sdb bs=4M status=progress
sync

```
### Kiểm tra trên BBB
```

# Nap driver
modprobe my_cdev

# Kiem tra file thiet bi
ls -l /dev/my_char_dev

# Test ghi du lieu vao Driver
echo "Hello Driver" > /dev/my_char_dev

# Xem Driver da nhan duoc chua
dmesg | tail

# Test doc du lieu tu Driver
cat /dev/my_char_dev
```
### Kết quả
![Kết quả cuối cùng](images/1.char_dev.jpg)