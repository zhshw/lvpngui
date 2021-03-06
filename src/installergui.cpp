#include "installergui.h"
#include "config.h"

#include <QMessageBox>

InstallerGUI::InstallerGUI(Installer &installer, QLockFile &lockFile, QObject *parent)
    : QObject(parent)
    , m_installer(installer)
    , m_lockFile(lockFile)
{}

void InstallerGUI::run() {
    // m_lockFile will need it before install() is called
    if (!m_installer.getDir().exists()) {
        m_installer.getDir().mkpath(".");
    }

    Installer::State installState = m_installer.detectState();

    QString already_running(tr("%1 is already running.").arg(VpnFeatures::name));

    if (installState == Installer::NotInstalled) {
        while (!m_lockFile.tryLock(100)) {
            // If we can't lock:
            // NotInstalled and already running means it's an upgrade.
            // We ask to close the older version so we can do the upgrade
            QString text(tr("Please close it and retry. "));
            auto r = QMessageBox::warning(nullptr, already_running,
                                          already_running + " " + text,
                                          QMessageBox::Cancel | QMessageBox::Retry,
                                          QMessageBox::Retry);

            if (r == QMessageBox::Cancel) {
                throw SilentError();
            }
        }

        m_installer.install();
        QString msg(tr("%1 is now installed! (version %2)"));
        msg = msg.arg(VpnFeatures::display_name, VPNGUI_VERSION);
        msg += "\n";
        msg += tr("See the system tray icon to connect.");
        QMessageBox::information(nullptr, tr("Installed"), msg, QMessageBox::Ok);
        // TODO: do we start the new process here and throw SilentError()?
    }
    else if (installState == Installer::HigherVersionFound) {
        throw InitializationError(QString(VpnFeatures::name), tr("%1 is already installed with a higher version.").arg(VpnFeatures::name));
    }
    else {
        // The same version is installed
        if (!m_lockFile.tryLock(100)) {
            // The same version is already running.
            throw InitializationError(QString(VpnFeatures::name), already_running);
        }
    }
}

void InstallerGUI::runUninstall() {
    while (!m_lockFile.tryLock(100)) {
        QString text(tr("Please close it and retry. "));
        auto r = QMessageBox::warning(nullptr, QString(VpnFeatures::display_name),
                                      text,
                                      QMessageBox::Cancel | QMessageBox::Retry,
                                      QMessageBox::Retry);

        if (r == QMessageBox::Cancel) {
            throw SilentError();
        }
    }

    m_installer.uninstall();

    QString msg(tr("%1 has been uninstalled."));
    msg += "\n";
    msg += tr("Do you want to uninstall TAP-Windows too? (may be used by other VPN softwares)");
    msg = msg.arg(VpnFeatures::display_name);
    auto r = QMessageBox::information(nullptr, tr("Uninstalled"), msg, QMessageBox::Yes | QMessageBox::No);

    if (r == QMessageBox::No) {
        return;
    }

    m_installer.uninstallTAP();
}

void InstallerGUI::runCheckInstall() {
    QString status("unknown");
    Installer::State installState = m_installer.detectState();
    if (installState == Installer::NotInstalled) {
        status = "Not installed";
    } else if (installState == Installer::Installed) {
        status = "Installed and up to date";
    } else if (installState == Installer::HigherVersionFound) {
        status = "Installed, upgrade possible";
    }

    QString title("Installation Status");
    QString text;
    text.append(QString("Status: %1\n").arg(status));
    text.append(QString("GUID: %1\n").arg(m_installer.getGuid()));
    text.append(QString("Dir: %1\n").arg(m_installer.getDir().path()));

    QMessageBox mb(QMessageBox::Information, title,
                   text, QMessageBox::Ok);
    mb.setTextInteractionFlags(Qt::TextSelectableByMouse);
    mb.exec();
}
