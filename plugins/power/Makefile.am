plugin_name = power

plugin_LTLIBRARIES =					\
	libpower.la

libpower_la_SOURCES = 					\
	gpm-common.c					\
	gpm-common.h					\
	gpm-phone.c					\
	gpm-phone.h					\
	gpm-idletime.c					\
	gpm-idletime.h					\
	gsd-power-manager.c				\
	gsd-power-manager.h				\
	gsd-power-plugin.c				\
	gsd-power-plugin.h

libpower_la_CPPFLAGS = \
	-I$(top_srcdir)/data/				\
	-I$(top_srcdir)/gnome-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"		\
	-DSBINDIR=\"$(sbindir)\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	$(AM_CPPFLAGS)

libpower_la_CFLAGS = \
	$(PLUGIN_CFLAGS)				\
	$(KEYRING_CFLAGS)				\
	$(UPOWER_CFLAGS)				\
	$(GNOME_DESKTOP_CFLAGS)				\
	$(SETTINGS_PLUGIN_CFLAGS)			\
	$(AM_CFLAGS)

libpower_la_LDFLAGS = 					\
	$(GSD_PLUGIN_LDFLAGS)

libpower_la_LIBADD  = 					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(UPOWER_LIBS)					\
	$(GNOME_DESKTOP_LIBS)				\
	$(KEYRING_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 					\
	power.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

org.gnome.settings-daemon.plugins.power.policy.in: org.gnome.settings-daemon.plugins.power.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.gnome.settings-daemon.plugins.power.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

libexec_PROGRAMS =					\
	gsd-backlight-helper

gsd_backlight_helper_SOURCES =				\
	gsd-backlight-helper.c

gsd_backlight_helper_LDFLAGS =				\
	$(BACKLIGHT_HELPER_LIBS)				\
	-lm

gsd_backlight_helper_CFLAGS =				\
	$(BACKLIGHT_HELPER_CFLAGS)

EXTRA_DIST = 						\
	org.gnome.settings-daemon.plugins.power.policy.in.in \
	$(plugin_in_files)

clean-local:
	rm -f *~

CLEANFILES = 						\
	$(plugin_DATA)					\
	org.gnome.settings-daemon.plugins.power.policy	\
	org.gnome.settings-daemon.plugins.power.policy.in

@GSD_INTLTOOL_PLUGIN_RULE@
