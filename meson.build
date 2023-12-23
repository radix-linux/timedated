project('timedated', 'c',
    version: '1.0.2',
    license: 'GPLv2+',
    default_options: [
        'buildtype=debugoptimized',
        'warning_level=1',
        'c_std=gnu99',
    ],
    meson_version: '>= 0.56.0')

soversion = 3
current = 1
revision = 0
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

gnome = import('gnome')
i18n = import('i18n')

cc = meson.get_compiler('c')

# directories
prefix = get_option('prefix')
bindir = get_option('bindir')
datadir = get_option('datadir')
libexecdir = get_option('libexecdir')

# TODO: Get rid of these by including config.h where needed
add_project_arguments([
    '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
    '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
], language: 'c')

cdata = configuration_data()
cdata.set_quoted('GETTEXT_PACKAGE', meson.project_name())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_SYSCONF_DIR', get_option('sysconfdir'))
cdata.set_quoted('HWCLOCK_CONF', get_option('hwclock_conf'))
cdata.set_quoted('ADJTIME_CONF', get_option('adjtime_conf'))
cdata.set_quoted('NTPD_CONF', get_option('ntpd_conf'))
cdata.set_quoted('NTPD_RC', get_option('ntpd_rc'))

glib_min_version    = '2.76'
polkit_min_version  = '123'
pcre_min_version    = '10.36'

glib_version_def = 'GLIB_VERSION_@0@_@1@'.format(
    glib_min_version.split('.')[0], glib_min_version.split('.')[1])
common_cflags = cc.get_supported_arguments([
    '-DGLIB_VERSION_MIN_REQUIRED=' + glib_version_def,
    '-DGLIB_VERSION_MAX_ALLOWED=' + glib_version_def,
])
add_project_arguments(common_cflags, language: 'c')


glib_dep = dependency('glib-2.0', version: '>=' + glib_min_version)
gobject_dep = dependency('gobject-2.0', version: '>=' + glib_min_version)
gio_dep = dependency('gio-2.0', version: '>=' + glib_min_version)
gio_unix_dep = dependency('gio-unix-2.0', version: '>=' + glib_min_version)
pcre_dep = dependency('libpcre2-8', version: '>=' + pcre_min_version)
polkit_dep = dependency('polkit-gobject-1', version: '>=' + polkit_min_version)
m_dep = cc.find_library('m', required: true)

dbusdir = join_paths(datadir, 'dbus-1')
polkitactionsdir = join_paths(datadir, 'polkit-1', 'actions')
polkitrulesdir = join_paths(datadir, 'polkit-1', 'rules.d')

# Generate configuration file
config_h = configure_file(output: 'config.h', configuration: cdata)

subdir('po')
subdir('dbus')
subdir('src')

output = []
output += 'timedated ' + meson.project_version()
output += 'System Paths'
output += '  prefix:         ' + get_option('prefix')
output += '  libdir:         ' + get_option('libdir')
output += '  libexecdir:     ' + get_option('prefix') / get_option('libexecdir')
output += '  bindir:         ' + get_option('prefix') / get_option('bindir')
output += '  sbindir:        ' + get_option('prefix') / get_option('sbindir')
output += '  datadir:        ' + get_option('prefix') / get_option('datadir')
output += '  sysconfdir:     ' + get_option('sysconfdir')
output += '  localstatedir:  ' + get_option('prefix') / get_option('localstatedir')

output += '\nFeatures'
output += '  Priviledged group:      ' + get_option('privileged-group')
output += '  Hardware clock config:  ' + get_option('hwclock_conf')
output += '  Adjtime config:         ' + get_option('adjtime_conf')
output += '  NTP  daemon config:     ' + get_option('ntpd_conf')
output += '  NTPd start/stop script: ' + get_option('ntpd_rc')

message('\n'+'\n'.join(output)+'\n')
