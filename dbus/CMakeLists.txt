# install the dbus interface xml file into the correct place
install(
    FILES org.kde.kio.StashNotifier.xml
    DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR}
)

install(
    FILES org.kde.kio.StashNotifier.service
    DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)
