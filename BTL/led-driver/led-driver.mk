LED_DRIVER_VERSION = 1.0
LED_DRIVER_SITE = $(LED_DRIVER_PKGDIR)/src
LED_DRIVER_SITE_METHOD = local

# Lệnh build module
define LED_DRIVER_BUILD_CMDS
	$(MAKE) -C $(LINUX_DIR) $(LINUX_MAKE_FLAGS) M=$(@D) modules
endef

# Lệnh copy file .ko vào hệ thống file của board
define LED_DRIVER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/led_driver.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/led_driver.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))
