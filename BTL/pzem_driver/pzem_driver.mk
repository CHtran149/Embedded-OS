PZEM_DRIVER_VERSION = 1.0
PZEM_DRIVER_SITE = $(PZEM_DRIVER_PKGDIR)/src
PZEM_DRIVER_SITE_METHOD = local

# Quan trọng: Thêm các biến LINUX_MAKE_FLAGS để Buildroot biết dùng cross-compiler
PZEM_DRIVER_MODULE_MAKE_OPTS = \
	$(LINUX_MAKE_FLAGS) \
	M=$(@D)

define PZEM_DRIVER_BUILD_CMDS
	$(MAKE) -C $(LINUX_DIR) $(PZEM_DRIVER_MODULE_MAKE_OPTS) modules
endef

define PZEM_DRIVER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/pzem_driver.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/extra/pzem_driver.ko
endef

$(eval $(kernel-module))
$(eval $(generic-package))
