// SPDX-License-Identifier: GPL-3.0-or-later

#include "daytonastyle.h"
#include "desktopfiles.h"
#include "desktopicons.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Daytona;

namespace {

void touch(const QString &path, const QByteArray &contents = "x")
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

QStringList names(const QList<DesktopItem> &items)
{
    QStringList list;
    for (const DesktopItem &item : items)
        list.append(item.name);
    return list;
}

} // namespace

class TestDesktopIcons : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // The view's default folder is the real Desktop folder; keep away
        // from the user's.
        QStandardPaths::setTestModeEnabled(true);
        auto *style = new Style;
        style->setColourFile(m_colours.filePath(QStringLiteral("colors.ini")));
        QApplication::setStyle(style);
    }

    void grid()
    {
        // 3 rows fit in 250 pixels: down the first column, then the next.
        const QRect area(10, 20, 400, 250);
        QCOMPARE(desktopGridCell(area, 0), QPoint(10, 20));
        QCOMPARE(desktopGridCell(area, 2), QPoint(10, 170));
        QCOMPARE(desktopGridCell(area, 3), QPoint(85, 20));
        QCOMPARE(nearestGridCell(area, QPoint(80, 50)), QPoint(85, 20));
        QCOMPARE(nearestGridCell(area, QPoint(40, 60)), QPoint(10, 95));
        // Outside: the nearest cell inside.
        QCOMPARE(nearestGridCell(area, QPoint(-500, 9000)), QPoint(10, 170));
    }

    void readFolder()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("b.txt")));
        touch(dir.filePath(QStringLiteral(".hidden")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("A folder")));
        touch(dir.filePath(QStringLiteral("app.desktop")),
            "[Desktop Entry]\nType=Application\nName=My Program\nIcon=my-program\nExec=true\n");
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("b.txt")),
            dir.filePath(QStringLiteral("Shortcut to b.txt"))));

        const QList<DesktopItem> items = readDesktopFolder(dir.path(), QStringLiteral("C"));
        QCOMPARE(names(items),
            QStringList({ QStringLiteral("A folder"), QStringLiteral("My Program"),
                QStringLiteral("b.txt"), QStringLiteral("Shortcut to b.txt") }));
        QVERIFY(items.at(0).isDir);
        QCOMPARE(items.at(0).iconName, QStringLiteral("folder"));
        // A .desktop file shows its own name and icon, keyed by file name.
        QVERIFY(items.at(1).isDesktopEntry);
        QCOMPARE(items.at(1).iconName, QStringLiteral("my-program"));
        QCOMPARE(items.at(1).key(), QStringLiteral("app.desktop"));
        QVERIFY(items.at(3).isLink);
        QCOMPARE(DesktopItem::system(DesktopItem::Kind::RecycleBin).key(),
            QStringLiteral("/recyclebin"));
    }

    void uniqueNames()
    {
        QTemporaryDir dir;
        const QString first = QStringLiteral("Copy of %1");
        const QString numbered = QStringLiteral("Copy (%2) of %1");
        QCOMPARE(uniqueName(dir.path(), QStringLiteral("a.txt"), first, numbered),
            QStringLiteral("a.txt"));
        touch(dir.filePath(QStringLiteral("a.txt")));
        QCOMPARE(uniqueName(dir.path(), QStringLiteral("a.txt"), first, numbered),
            QStringLiteral("Copy of a.txt"));
        touch(dir.filePath(QStringLiteral("Copy of a.txt")));
        QCOMPARE(uniqueName(dir.path(), QStringLiteral("a.txt"), first, numbered),
            QStringLiteral("Copy (2) of a.txt"));
    }

    void clipboardFormats()
    {
        const QList<QUrl> urls { QUrl::fromLocalFile(QStringLiteral("/tmp/a b")),
            QUrl::fromLocalFile(QStringLiteral("/tmp/c")) };
        std::unique_ptr<QMimeData> cut(fileClipboardData(urls, true));
        QCOMPARE(cut->urls(), urls);
        QCOMPARE(cut->data(QStringLiteral("x-special/gnome-copied-files")),
            QByteArray("cut\nfile:///tmp/a%20b\nfile:///tmp/c"));
        ClipboardFiles read = readFileClipboard(cut.get());
        QVERIFY(read.cut);
        QCOMPARE(read.urls, urls);

        // A file manager's copy, as PCManFM-Qt writes it; and a plain URL
        // list, which is a copy. Remote URLs are left out.
        QMimeData copied;
        copied.setData(
            QStringLiteral("x-special/gnome-copied-files"), "copy\nfile:///tmp/c\nsftp://host/x\n");
        read = readFileClipboard(&copied);
        QVERIFY(!read.cut);
        QCOMPARE(read.urls, QList<QUrl>({ QUrl::fromLocalFile(QStringLiteral("/tmp/c")) }));
        QMimeData plain;
        plain.setUrls(urls);
        read = readFileClipboard(&plain);
        QVERIFY(!read.cut);
        QCOMPARE(read.urls, urls);
        QVERIFY(readFileClipboard(nullptr).urls.isEmpty());
    }

    void paste()
    {
        QTemporaryDir from;
        QTemporaryDir to;
        touch(from.filePath(QStringLiteral("a.txt")), "a");
        QVERIFY(QDir(from.path()).mkpath(QStringLiteral("dir/sub")));
        touch(from.filePath(QStringLiteral("dir/sub/f")), "f");
        touch(to.filePath(QStringLiteral("a.txt")), "old");

        // Copying never replaces: the taken name becomes "Copy of".
        PasteResult result
            = pasteFiles({ QUrl::fromLocalFile(from.filePath(QStringLiteral("a.txt"))),
                             QUrl::fromLocalFile(from.filePath(QStringLiteral("dir"))) },
                to.path(), false);
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join(u'\n')));
        QCOMPARE(result.created,
            QStringList({ QStringLiteral("Copy of a.txt"), QStringLiteral("dir") }));
        QVERIFY(QFile::exists(to.filePath(QStringLiteral("dir/sub/f"))));
        QVERIFY(QFile::exists(from.filePath(QStringLiteral("a.txt"))));
        QFile old(to.filePath(QStringLiteral("a.txt")));
        QVERIFY(old.open(QIODevice::ReadOnly));
        QCOMPARE(old.readAll(), QByteArray("old"));

        // Cut and paste moves.
        result = pasteFiles(
            { QUrl::fromLocalFile(from.filePath(QStringLiteral("dir"))) }, to.path(), true);
        QCOMPARE(result.created, QStringList({ QStringLiteral("Copy of dir") }));
        QVERIFY(!QFile::exists(from.filePath(QStringLiteral("dir"))));

        // Moving into the same folder does nothing; copying makes a copy.
        const QUrl inPlace = QUrl::fromLocalFile(to.filePath(QStringLiteral("a.txt")));
        QVERIFY(pasteFiles({ inPlace }, to.path(), true).created.isEmpty());
        QCOMPARE(pasteFiles({ inPlace }, to.path(), false).created,
            QStringList({ QStringLiteral("Copy (2) of a.txt") }));

        // A folder cannot be copied into itself.
        result = pasteFiles(
            { QUrl::fromLocalFile(to.path()) }, to.filePath(QStringLiteral("Copy of dir")), false);
        QVERIFY(result.created.isEmpty());
        QCOMPARE(result.errors.size(), 1);
    }

    void shortcuts()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.txt")));
        const QStringList made
            = createShortcuts({ dir.filePath(QStringLiteral("a.txt")) }, dir.path());
        QCOMPARE(made, QStringList({ QStringLiteral("Shortcut to a.txt") }));
        QCOMPARE(QFileInfo(dir.filePath(made.first())).symLinkTarget(),
            dir.filePath(QStringLiteral("a.txt")));
        QCOMPARE(createShortcuts({ dir.filePath(QStringLiteral("a.txt")) }, dir.path()),
            QStringList({ QStringLiteral("Shortcut (2) to a.txt") }));
    }

    void trash()
    {
        QTemporaryDir dir;
        const QString files = dir.filePath(QStringLiteral("Trash/files"));
        const QString info = dir.filePath(QStringLiteral("Trash/info"));
        QVERIFY(QDir().mkpath(files));
        QVERIFY(QDir().mkpath(info));
        QVERIFY(!trashIsFull(files));
        touch(files + QStringLiteral("/a"));
        QVERIFY(QDir().mkpath(files + QStringLiteral("/d/e")));
        touch(info + QStringLiteral("/a.trashinfo"));
        touch(info + QStringLiteral("/d.trashinfo"));
        QVERIFY(trashIsFull(files));
        QVERIFY(emptyTrash(files).isEmpty());
        QVERIFY(!trashIsFull(files));
        QVERIFY(QDir(info).isEmpty());
    }

    void settingsPositions()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        QVERIFY(settings.desktopIconPositions().isEmpty());
        const QHash<QString, QPoint> positions { { QStringLiteral("/computer"), QPoint(0, 0) },
            { QStringLiteral("a, b.txt"), QPoint(75, 150) } };
        settings.setDesktopIconPositions(positions);
        Settings again(dir.filePath(QStringLiteral("daytona.ini")));
        QCOMPARE(again.desktopIconPositions(), positions);
        QVERIFY(!again.desktopAutoArrange());
        again.setDesktopAutoArrange(true);
        QVERIFY(Settings(dir.filePath(QStringLiteral("daytona.ini"))).desktopAutoArrange());
    }

    void view()
    {
        QTemporaryDir dir;
        const QString desktop = dir.filePath(QStringLiteral("Desktop"));
        const QString trashFiles = dir.filePath(QStringLiteral("Trash/files"));
        QVERIFY(QDir().mkpath(desktop));
        touch(desktop + QStringLiteral("/b.txt"));
        touch(desktop + QStringLiteral("/a.txt"));
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        QAction properties(QStringLiteral("Properties"));

        DesktopIconView view(&settings, &properties);
        view.resize(640, 480);
        view.setMargins(QMargins(0, 0, 0, 30));
        QSignalSpy loaded(&view, &DesktopIconView::loaded);
        view.setDirectory(desktop, trashFiles);
        QVERIFY(loaded.wait());
        // NT's icons first, then the folder by name, down the first column.
        QCOMPARE(names(view.items()),
            QStringList({ QStringLiteral("My Computer"), QStringLiteral("Network Neighborhood"),
                QStringLiteral("Recycle Bin"), QStringLiteral("a.txt"), QStringLiteral("b.txt") }));
        QCOMPARE(view.position(QStringLiteral("/computer")), QPoint(0, 0));
        QCOMPARE(view.position(QStringLiteral("b.txt")), QPoint(0, 4 * DesktopIconView::CellSize));
        QVERIFY(view.desktopMenu()->actions().contains(&properties));

        // A file added by another program appears without being asked.
        touch(desktop + QStringLiteral("/c.txt"));
        QTRY_VERIFY(names(view.items()).contains(QStringLiteral("c.txt")));
        // 450 pixels hold 6 rows: the next goes to the second column.
        QCOMPARE(view.position(QStringLiteral("c.txt")), QPoint(0, 5 * DesktopIconView::CellSize));
        touch(desktop + QStringLiteral("/d.txt"));
        QTRY_VERIFY(names(view.items()).contains(QStringLiteral("d.txt")));
        QCOMPARE(view.position(QStringLiteral("d.txt")), QPoint(DesktopIconView::CellSize, 0));

        // The Recycle Bin shows when it has something in it.
        touch(trashFiles + QStringLiteral("/old"));
        QTRY_COMPARE(view.items().at(2).iconName, QStringLiteral("user-trash-full"));

        // Menus: the Recycle Bin's can empty it; a file's has the file
        // commands, Open first and default.
        QMenu *bin = view.itemMenu(QStringLiteral("/recyclebin"));
        QVERIFY(bin->findChild<QAction *>(QStringLiteral("empty"))->isEnabled());
        delete bin;
        QMenu *file = view.itemMenu(QStringLiteral("a.txt"));
        QCOMPARE(file->defaultAction()->objectName(), QStringLiteral("open"));
        for (const char *name : { "cut", "copy", "shortcut", "delete", "rename" })
            QVERIFY2(file->findChild<QAction *>(QLatin1String(name)), name);
        delete file;
    }

    void arrangeAndRemember()
    {
        QTemporaryDir dir;
        const QString desktop = dir.filePath(QStringLiteral("Desktop"));
        QVERIFY(QDir().mkpath(desktop));
        touch(desktop + QStringLiteral("/small.txt"), "1");
        touch(desktop + QStringLiteral("/big.txt"), "123456789");
        const QString ini = dir.filePath(QStringLiteral("daytona.ini"));
        Settings settings(ini);
        // Placed by the user somewhere earlier.
        settings.setDesktopIconPositions({ { QStringLiteral("small.txt"), QPoint(300, 200) } });

        DesktopIconView view(&settings, nullptr);
        view.resize(640, 480);
        QSignalSpy loaded(&view, &DesktopIconView::loaded);
        view.setDirectory(desktop, dir.filePath(QStringLiteral("Trash/files")));
        QVERIFY(loaded.wait());
        QCOMPARE(view.position(QStringLiteral("small.txt")), QPoint(300, 200));
        // big.txt takes the next free place after NT's three icons.
        QCOMPARE(
            view.position(QStringLiteral("big.txt")), QPoint(0, 3 * DesktopIconView::CellSize));

        // Line up Icons moves it to the nearest grid cell.
        view.lineUp();
        QCOMPARE(view.position(QStringLiteral("small.txt")), QPoint(300, 225));

        // Arrange by Size puts the smaller first, after NT's icons, and is
        // remembered.
        view.arrange(DesktopIconView::BySize);
        QCOMPARE(
            view.position(QStringLiteral("small.txt")), QPoint(0, 3 * DesktopIconView::CellSize));
        QCOMPARE(
            view.position(QStringLiteral("big.txt")), QPoint(0, 4 * DesktopIconView::CellSize));
        QCOMPARE(Settings(ini).desktopIconPositions().value(QStringLiteral("big.txt")),
            QPoint(0, 4 * DesktopIconView::CellSize));
    }

    void rename()
    {
        QTemporaryDir dir;
        const QString desktop = dir.filePath(QStringLiteral("Desktop"));
        QVERIFY(QDir().mkpath(desktop));
        touch(desktop + QStringLiteral("/old.txt"));
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DesktopIconView view(&settings, nullptr);
        view.resize(640, 480);
        QSignalSpy loaded(&view, &DesktopIconView::loaded);
        view.setDirectory(desktop, dir.filePath(QStringLiteral("Trash/files")));
        QVERIFY(loaded.wait());
        const QPoint before = view.position(QStringLiteral("old.txt"));

        // Typing a letter selects the icon; F2 renames it.
        QTest::keyClick(&view, Qt::Key_O);
        QCOMPARE(view.selectedKeys(), QStringList({ QStringLiteral("old.txt") }));
        QMenu *menu = view.itemMenu(QStringLiteral("old.txt"));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("rename"))->isEnabled());
        delete menu;
        QTest::keyClick(&view, Qt::Key_F2);
        auto *editor = view.findChild<QLineEdit *>(QStringLiteral("rename"));
        QVERIFY(editor);
        // The name without its extension is selected.
        QCOMPARE(editor->selectedText(), QStringLiteral("old"));
        editor->setText(QStringLiteral("new.txt"));
        QTest::keyClick(editor, Qt::Key_Return);
        QVERIFY(QFile::exists(desktop + QStringLiteral("/new.txt")));
        QTRY_VERIFY(names(view.items()).contains(QStringLiteral("new.txt")));
        // It stays where it was.
        QCOMPARE(view.position(QStringLiteral("new.txt")), before);
    }

private:
    QTemporaryDir m_colours;
};

QTEST_MAIN(TestDesktopIcons)
#include "tst_desktopicons.moc"
