
timedated_deps = declare_dependency(
    include_directories: [
        include_directories('.'),
        include_directories('..'),
        include_directories('../dbus'),
    ],
    dependencies: [
        m_dep, glib_dep, gobject_dep, gio_dep, gio_unix_dep, pcre_dep, polkit_dep, timedated_dbus_dep
    ],
    compile_args: [
        '-DUP_COMPILATION',
    ],
)

timedated_private = static_library('timedated-private',
    sources: [
        'rcl-timedate.h',
        'rcl-timedate.c',
        'rcl-time-utils.h',
        'rcl-time-utils.c',
        'rcl-ntpd-utils.h',
        'rcl-ntpd-utils.c',
        'rcl-zone-utils.h',
        'rcl-zone-utils.c',
    ],
    dependencies: [ timedated_deps ],
    c_args: [ '-DG_LOG_DOMAIN="Timedate"' ],
)

timedated = executable('timedated',
    sources: [
        'rcl-main.c',
    ],
    dependencies: timedated_deps,
    link_with: [ timedated_private ],
    gnu_symbol_visibility: 'hidden',
    install: true,
    install_dir: get_option('prefix') / get_option('libexecdir'),
    c_args: [ '-DG_LOG_DOMAIN="Timedate"' ],
)


#####################
# Data/Config files:
#####################

cdata = configuration_data()
cdata.set('libexecdir', get_option('prefix') / get_option('libexecdir'))
cdata.set('PRIVILEDGED_GROUP', get_option('privileged-group'))

configure_file(
    input: 'org.freedesktop.timedate1.service.in',
    output: 'org.freedesktop.timedate1.service',
    install_dir: dbusdir / 'system-services',
    configuration: cdata,
)

configure_file(
    input: 'org.freedesktop.timedate1.conf.in',
    output: 'org.freedesktop.timedate1.conf',
    install_dir: dbusdir / 'system.d',
    configuration: cdata,
)

configure_file(
    input: 'org.freedesktop.timedate1.policy.in',
    output: 'org.freedesktop.timedate1.policy',
    install_dir: polkitactionsdir,
    configuration: cdata,
)

configure_file(
    input: 'org.freedesktop.timedate1.rules.in',
    output: 'org.freedesktop.timedate1.rules',
    install_dir: polkitrulesdir,
    configuration: cdata,
)
