SSD1306_DRIVER_VERSION = 1.0
SSD1306_DRIVER_SITE = $(TOPDIR)/package/ssd1306_driver/src
SSD1306_DRIVER_SITE_METHOD = local
SSD1306_DRIVER_LICENSE = GPL-2.0

# Chỉ định Kernel Directory và trình biên dịch
SSD1306_DRIVER_KDIR = $(LINUX_DIR)

define SSD1306_DRIVER_BUILD_CMDS
	$(MAKE) -C $(SSD1306_DRIVER_KDIR) M=$(@D) modules \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH=$(KERNEL_ARCH)
endef

define SSD1306_DRIVER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/ssd1306_driver.ko \
		$(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/ssd1306_driver.ko
endef

$(eval $(generic-package))
