// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "systeminfo.h"

#include <QDialog>
#include <functional>

class QGridLayout;
class QLabel;
class QProgressBar;
class QTabWidget;
class QTreeWidget;
class QWidget;

namespace Daytona {

// System Properties, from Control Panel > System and My Computer >
// Properties: the General tab of Windows 98's (a picture of a computer, then
// System, Registered to, Computer name and Computer) and a read-only
// Performance tab (memory and virtual memory). Everything is read once when
// the dialog opens; nothing needs root. NT's other tabs, Device Manager and
// Hardware Profiles are not offered.
class SystemPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SystemPropertiesDialog(QWidget *parent = nullptr);

    // What to show; readSystemInfo() unless replaced (tests).
    void setReader(const std::function<SystemInfo()> &read) { m_read = read; }

    // Reads the computer's details again.
    void prepare();

    QTabWidget *tabs() const { return m_tabs; }
    QLabel *logo() const { return m_logo; }
    // The General tab's text, line by line, for tests.
    QStringList generalText() const;
    QLabel *memoryText() const { return m_memory; }
    QLabel *memoryInUseText() const { return m_memoryInUse; }
    QLabel *swapText() const { return m_swap; }
    QProgressBar *memoryBar() const { return m_memoryBar; }
    QProgressBar *swapBar() const { return m_swapBar; }
    QTreeWidget *swapList() const { return m_swapList; }

private:
    QWidget *createGeneralPage();
    QWidget *createPerformancePage();

    std::function<SystemInfo()> m_read;
    QTabWidget *m_tabs;
    QWidget *m_details; // the General tab
    QLabel *m_picture;
    QLabel *m_logo; // beside Computer, as a computer maker's logo was
    QLabel *m_memory;
    QLabel *m_memoryInUse;
    QProgressBar *m_memoryBar;
    QLabel *m_swap;
    QLabel *m_swapInUseHeading;
    QLabel *m_swapInUse;
    QProgressBar *m_swapBar;
    QTreeWidget *m_swapList;
    QLabel *m_noSwap;
};

} // namespace Daytona
