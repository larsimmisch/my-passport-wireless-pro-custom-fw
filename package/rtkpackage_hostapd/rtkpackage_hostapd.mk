##############################################################
#
# rtkpackage_hostapd
#
#############################################################

RTKPACKAGE_HOSTAPD_VERSION = 2.0
#RTKPACKAGE_HOSTAPD_SITE = local
RTKPACKAGE_HOSTAPD_SUBDIR = hostapd
RTKPACKAGE_HOSTAPD_CONFIG = $(RTKPACKAGE_HOSTAPD_DIR)/$(RTKPACKAGE_HOSTAPD_SUBDIR)/.config
RTKPACKAGE_HOSTAPD_DEPENDENCIES = libnl
RTKPACKAGE_HOSTAPD_CFLAGS = $(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/include/libnl3/
RTKPACKAGE_HOSTAPD_LDFLAGS = $(TARGET_LDFLAGS)
RTKPACKAGE_HOSTAPD_LICENSE = GPLv2/BSD-3c
RTKPACKAGE_HOSTAPD_LICENSE_FILES = README
RTKPACKAGE_HOSTAPD_CONF_ENV = LIBS="-lnl -lnl-genl"

# libnl needs -lm (for rint) if linking statically
ifeq ($(BR2_PREFER_STATIC_LIB),y)
RTKPACKAGE_HOSTAPD_LDFLAGS += -lm
endif

define RTKPACKAGE_HOSTAPD_LIBNL_CONFIG
	echo '' >> $(RTKPACKAGE_HOSTAPD_CONFIG)
	echo 'CONFIG_LIBNL32=y' >> $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_VLAN_NETLINK.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef

define RTKPACKAGE_HOSTAPD_LIBTOMMATH_CONFIG
	$(SED) 's/\(#\)\(CONFIG_INTERNAL_LIBTOMMATH.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef

# Try to use openssl if it's already available
ifeq ($(BR2_PACKAGE_OPENSSL),y)
	RTKPACKAGE_HOSTAPD_DEPENDENCIES += openssl
define RTKPACKAGE_HOSTAPD_TLS_CONFIG
	$(SED) 's/\(#\)\(CONFIG_TLS=openssl\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_PWD.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
else
define RTKPACKAGE_HOSTAPD_TLS_CONFIG
	$(SED) 's/\(#\)\(CONFIG_TLS=\).*/\2internal/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
endif

ifeq ($(BR2_PACKAGE_HOSTAPD_EAP),y)
define RTKPACKAGE_HOSTAPD_EAP_CONFIG
	$(SED) 's/\(#\)\(CONFIG_EAP_AKA.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_FAST.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_GPSK.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_IKEV2.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_PAX.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_PSK.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_SAKE.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_SIM.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_EAP_TNC.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_RADIUS_SERVER.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_TLSV1.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
ifneq ($(BR2_INET_IPV6),y)
define RTKPACKAGE_HOSTAPD_RADIUS_IPV6_CONFIG
	$(SED) 's/\(CONFIG_IPV6.*\)/#\1/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
endif
else
define RTKPACKAGE_HOSTAPD_EAP_CONFIG
	$(SED) 's/^\(CONFIG_EAP.*\)/#\1/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_NO_ACCOUNTING.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_NO_RADIUS.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
endif

ifeq ($(BR2_PACKAGE_HOSTAPD_WPS),y)
define RTKPACKAGE_HOSTAPD_WPS_CONFIG
	$(SED) 's/\(#\)\(CONFIG_WPS.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
endef
endif

define RTKPACKAGE_HOSTAPD_CONFIGURE_CMDS
#	cp $(@D)/hostapd/defconfig $(RTKPACKAGE_HOSTAPD_CONFIG)
# Misc
	$(SED) 's/\(#\)\(CONFIG_HS20.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_IEEE80211N.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_IEEE80211R.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_IEEE80211W.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_INTERWORKING.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(SED) 's/\(#\)\(CONFIG_FULL_DYNAMIC_VLAN.*\)/\2/' $(RTKPACKAGE_HOSTAPD_CONFIG)
	$(RTKPACKAGE_HOSTAPD_LIBTOMMATH_CONFIG)
	$(RTKPACKAGE_HOSTAPD_TLS_CONFIG)
	$(RTKPACKAGE_HOSTAPD_RADIUS_IPV6_CONFIG)
	$(RTKPACKAGE_HOSTAPD_EAP_CONFIG)
	$(RTKPACKAGE_HOSTAPD_WPS_CONFIG)
	$(RTKPACKAGE_HOSTAPD_LIBNL_CONFIG)
endef

define RTKPACKAGE_HOSTAPD_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(RTKPACKAGE_HOSTAPD_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) CC="$(TARGET_CC)" -C $(@D)/$(RTKPACKAGE_HOSTAPD_SUBDIR)
endef

define RTKPACKAGE_HOSTAPD_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/$(RTKPACKAGE_HOSTAPD_SUBDIR)/hostapd \
		$(TARGET_DIR)/usr/sbin/hostapd
	$(INSTALL) -m 0755 -D $(@D)/$(RTKPACKAGE_HOSTAPD_SUBDIR)/hostapd_cli \
		$(TARGET_DIR)/usr/bin/hostapd_cli
	#$(INSTALL) -D -m 755 package/hostapd/hostapd.conf \
	#	$(TARGET_DIR)/etc/hostapd/hostapd.conf
	#$(INSTALL) -D -m 755 package/hostapd/hostapd.conf \
	#	$(TARGET_DIR)/etc/hostapd/hostapd.default.conf
	$(INSTALL) -D -m 755 package/rtkpackage_hostapd/hostapd.conf.2G \
		$(TARGET_DIR)/etc/hostapd/hostapd.conf.2G
	$(INSTALL) -D -m 755 package/rtkpackage_hostapd/hostapd.conf.5G \
		$(TARGET_DIR)/etc/hostapd/hostapd.conf.5G
	$(INSTALL) -D -m 755 package/rtkpackage_hostapd/entropy.bin \
		$(TARGET_DIR)/etc/hostapd/entropy.bin
endef

#define RTKPACKAGE_HOSTAPD_INSTALL_INIT_SYSV 
#	$(INSTALL) -D -m 755 package/hostapd/S60hostapd \
#		$(TARGET_DIR)/etc/init.d/S60hostapd
#	$(INSTALL) -D -m 755 package/hostapd/S90multi-role \
#		$(TARGET_DIR)/etc/init.d/S90multi-role
#endef

$(eval $(generic-package))
