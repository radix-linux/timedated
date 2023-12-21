
timedated_dbus_interfaces = [
    [ 'timedate', 'org.freedesktop.timedate1', 'Daemon' ],
]

timedated_dbus_headers = []
timedated_dbus_sources = []
foreach interface: timedated_dbus_interfaces
    xml = interface[1] + '.xml'
    t = gnome.gdbus_codegen('rcl-' + interface[0] + '-generated',
        sources: xml,
        autocleanup: 'all',
        annotations:[ [ interface[1], 'org.gtk.GDBus.C.Name', 'Timedate' + interface[2] ] ],
        namespace: 'Rcl',
        object_manager: false,
    )
    timedated_dbus_sources += t[0]
    timedated_dbus_headers += t[1]

    install_data(xml,
        install_dir: dbusdir / 'interfaces',
    )
endforeach


timedated_dbus = static_library('libtimedate-dbus',
    sources: timedated_dbus_sources + timedated_dbus_headers,
    dependencies: [ gobject_dep, gio_dep, gio_unix_dep ],
)

timedated_dbus_dep = declare_dependency(
    link_with: timedated_dbus,
    include_directories: [ '.' ],
    sources: timedated_dbus_headers,
    dependencies: [ gio_unix_dep ]
)
