BTL_VERSION = 1.0
BTL_SITE = $(TOPDIR)/package/BTL/src
BTL_SITE_METHOD = local

BTL_DEPENDENCIES = mosquitto
define BTL_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define BTL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/btl_app $(TARGET_DIR)/usr/bin/btl_app
endef

$(eval $(generic-package))
