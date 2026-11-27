// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdgmenu.h"

#include <QDir>
#include <QDirIterator>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QMap>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

Q_LOGGING_CATEGORY(lcMenu, "daytona.menu", QtInfoMsg)

namespace Daytona {

const char *const BuiltInMenuFile = ":/daytona/daytona-applications.menu";

namespace {

    // Menu files are small. Merging can nest, but never this deep in a
    // sensible setup; the limit stops runaway merges.
    constexpr qint64 MaxMenuFileSize = 1024 * 1024;
    constexpr int MaxMergeDepth = 16;

    const QString TagMenu = QStringLiteral("Menu");
    const QString TagName = QStringLiteral("Name");

    QString text(const QDomElement &element)
    {
        return element.text().trimmed();
    }

    QString childText(const QDomElement &element, const QString &tag)
    {
        return text(element.firstChildElement(tag));
    }

    QList<QDomElement> childElements(const QDomElement &element, const QString &tag = {})
    {
        QList<QDomElement> out;
        for (QDomElement child = element.firstChildElement(tag); !child.isNull();
            child = child.nextSiblingElement(tag)) {
            out.append(child);
        }
        return out;
    }

    QString absolutePath(const QString &base, const QString &path)
    {
        if (path.isEmpty())
            return {};
        return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : base + QLatin1Char('/') + path);
    }

    QDomElement textElement(QDomDocument &doc, const QString &tag, const QString &value)
    {
        QDomElement element = doc.createElement(tag);
        element.appendChild(doc.createTextNode(value));
        return element;
    }

    void setText(QDomElement &element, const QString &value)
    {
        while (element.hasChildNodes())
            element.removeChild(element.firstChild());
        element.appendChild(element.ownerDocument().createTextNode(value));
    }

    // Moves every child of from, except its <Name>, to the end of to.
    void moveContents(QDomElement &from, QDomElement &to)
    {
        QDomNode child = from.firstChild();
        while (!child.isNull()) {
            QDomNode next = child.nextSibling();
            if (!(child.isElement() && child.toElement().tagName() == TagName))
                to.appendChild(child);
            child = next;
        }
    }

    QDomElement childMenu(const QDomElement &menu, const QString &name)
    {
        for (const QDomElement &child : childElements(menu, TagMenu)) {
            if (childText(child, TagName) == name)
                return child;
        }
        return {};
    }

    bool isMenuFile(const QString &path)
    {
        return path.startsWith(QLatin1Char(':')) || QFileInfo(path).isFile();
    }

    // Reads menu files and resolves everything that refers to other files:
    // merges and default directories. Relative paths are relative to the
    // file that names them, so this happens per file, before merging.
    class Loader
    {
    public:
        Loader(const XdgMenuPaths &paths, XdgMenu *out)
            : m_paths(paths)
            , m_out(out)
        {
        }

        // The resolved top-level <Menu> of path, owned by target but not in
        // it yet, or a null element.
        QDomElement load(const QString &path, QDomDocument &target, int depth = 0)
        {
            if (depth > MaxMergeDepth) {
                qCWarning(lcMenu) << "menu merges nest too deeply at" << path;
                return {};
            }
            const QString key
                = path.startsWith(QLatin1Char(':')) ? path : QFileInfo(path).canonicalFilePath();
            if (key.isEmpty() || m_loading.contains(key))
                return {};
            QFile file(path);
            if (file.size() > MaxMenuFileSize || !file.open(QIODevice::ReadOnly))
                return {};
            if (!path.startsWith(QLatin1Char(':')))
                m_out->watchFiles.append(path);
            QDomDocument doc;
            const QDomDocument::ParseResult result = doc.setContent(&file);
            if (!result) {
                qCWarning(lcMenu) << "cannot read menu" << path << "line" << result.errorLine
                                  << result.errorMessage;
                return {};
            }
            QDomElement root = doc.documentElement();
            if (root.tagName() != TagMenu)
                return {};
            m_loading.insert(key);
            resolve(root, path, depth);
            m_loading.remove(key);
            return target.importNode(root, true).toElement();
        }

    private:
        void resolve(QDomElement &menu, const QString &file, int depth)
        {
            QDomDocument doc = menu.ownerDocument();
            const QString base = QFileInfo(file).absolutePath();
            QDomElement child = menu.firstChildElement();
            while (!child.isNull()) {
                QDomElement next = child.nextSiblingElement();
                const QString tag = child.tagName();
                if (tag == QLatin1String("AppDir") || tag == QLatin1String("DirectoryDir")
                    || tag == QLatin1String("MergeDir")) {
                    setText(child, absolutePath(base, text(child)));
                }

                // Replaced by other elements, which are then resolved in
                // turn: the least important directory first, as later
                // elements win.
                QList<QDomElement> replacement;
                if (tag == QLatin1String("DefaultAppDirs")) {
                    for (auto it = m_paths.dataDirs.crbegin(); it != m_paths.dataDirs.crend(); ++it)
                        replacement << textElement(
                            doc, QStringLiteral("AppDir"), *it + QStringLiteral("/applications"));
                } else if (tag == QLatin1String("DefaultDirectoryDirs")) {
                    for (auto it = m_paths.dataDirs.crbegin(); it != m_paths.dataDirs.crend(); ++it)
                        replacement << textElement(doc, QStringLiteral("DirectoryDir"),
                            *it + QStringLiteral("/desktop-directories"));
                } else if (tag == QLatin1String("DefaultMergeDirs")) {
                    for (auto it = m_paths.configDirs.crbegin(); it != m_paths.configDirs.crend();
                        ++it)
                        replacement << textElement(doc, QStringLiteral("MergeDir"),
                            *it + QStringLiteral("/menus/applications-merged"));
                } else if (tag == QLatin1String("MergeDir")) {
                    const QString dir = text(child);
                    if (QFileInfo(dir).isDir()) {
                        m_out->watchDirs.append(dir);
                        QStringList files
                            = QDir(dir).entryList({ QStringLiteral("*.menu") }, QDir::Files);
                        files.sort();
                        for (const QString &name : std::as_const(files)) {
                            QDomElement merge = textElement(
                                doc, QStringLiteral("MergeFile"), dir + QLatin1Char('/') + name);
                            replacement << merge;
                        }
                    }
                } else if (tag == QLatin1String("MergeFile")) {
                    const QString path
                        = child.attribute(QStringLiteral("type")) == QLatin1String("parent")
                        ? parentMenu(file)
                        : absolutePath(base, text(child));
                    if (!path.isEmpty() && isMenuFile(path)) {
                        QDomElement merged = load(path, doc, depth + 1);
                        if (!merged.isNull()) {
                            // Its contents take the <MergeFile>'s place.
                            QDomNode node = merged.firstChild();
                            while (!node.isNull()) {
                                QDomNode after = node.nextSibling();
                                if (!(node.isElement() && node.toElement().tagName() == TagName))
                                    menu.insertBefore(node, child);
                                node = after;
                            }
                        }
                    }
                    menu.removeChild(child);
                } else if (tag == TagMenu) {
                    resolve(child, file, depth);
                } else if (tag == QLatin1String("LegacyDir")
                    || tag == QLatin1String("KDELegacyDirs")) {
                    // Deprecated in the specification; nothing uses them.
                    menu.removeChild(child);
                }

                if (tag == QLatin1String("DefaultAppDirs")
                    || tag == QLatin1String("DefaultDirectoryDirs")
                    || tag == QLatin1String("DefaultMergeDirs")
                    || tag == QLatin1String("MergeDir")) {
                    for (const QDomElement &element : std::as_const(replacement))
                        menu.insertBefore(element, child);
                    menu.removeChild(child);
                    if (!replacement.isEmpty())
                        next = replacement.first();
                }
                child = next;
            }
        }

        // <MergeFile type="parent">: the same menu file in the next, less
        // important, config directory; the built-in menu below them all.
        QString parentMenu(const QString &file) const
        {
            const QString path = QDir::cleanPath(QFileInfo(file).absoluteFilePath());
            for (int i = 0; i < m_paths.configDirs.size(); ++i) {
                const QString prefix = m_paths.configDirs[i] + QStringLiteral("/menus/");
                if (!path.startsWith(prefix))
                    continue;
                const QString relative = path.mid(prefix.size());
                for (int j = i + 1; j < m_paths.configDirs.size(); ++j) {
                    const QString candidate
                        = m_paths.configDirs[j] + QStringLiteral("/menus/") + relative;
                    if (QFileInfo(candidate).isFile())
                        return candidate;
                }
                if (relative.endsWith(QLatin1String("applications.menu")))
                    return QString::fromLatin1(BuiltInMenuFile);
                return {};
            }
            return {};
        }

        const XdgMenuPaths &m_paths;
        XdgMenu *m_out;
        QSet<QString> m_loading;
    };

    // Merges submenus with the same name (the later one's contents go after
    // the earlier one's) and drops repeated <AppDir>s and <DirectoryDir>s
    // but their last.
    void consolidate(QDomElement menu)
    {
        QHash<QString, QDomElement> byName;
        for (QDomElement child : childElements(menu, TagMenu)) {
            const QString name = childText(child, TagName);
            if (name.isEmpty()) {
                menu.removeChild(child);
                continue;
            }
            auto it = byName.find(name);
            if (it == byName.end()) {
                byName.insert(name, child);
            } else {
                moveContents(child, *it);
                menu.removeChild(child);
            }
        }
        for (const QString &tag : { QStringLiteral("AppDir"), QStringLiteral("DirectoryDir") }) {
            QSet<QString> seen;
            QList<QDomElement> dirs = childElements(menu, tag);
            for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) {
                if (seen.contains(text(*it)))
                    menu.removeChild(*it);
                else
                    seen.insert(text(*it));
            }
        }
        for (const QDomElement &child : childElements(menu, TagMenu))
            consolidate(child);
    }

    QDomElement findMenu(QDomElement menu, const QString &path)
    {
        for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            menu = childMenu(menu, part);
            if (menu.isNull())
                break;
        }
        return menu;
    }

    // <Move><Old>a/b</Old><New>c/d</New></Move>, relative to the menu that
    // has it, in order.
    void applyMoves(QDomElement menu)
    {
        QDomDocument doc = menu.ownerDocument();
        for (QDomElement move : childElements(menu, QStringLiteral("Move"))) {
            const QString from = childText(move, QStringLiteral("Old"));
            const QStringList to = childText(move, QStringLiteral("New"))
                                       .split(QLatin1Char('/'), Qt::SkipEmptyParts);
            menu.removeChild(move);
            QDomElement source = findMenu(menu, from);
            if (from.isEmpty() || source.isNull() || to.isEmpty() || source == menu)
                continue;
            source.parentNode().removeChild(source);
            QDomElement parent = menu;
            for (int i = 0; i + 1 < to.size(); ++i) {
                QDomElement next = childMenu(parent, to[i]);
                if (next.isNull()) {
                    next = doc.createElement(TagMenu);
                    next.appendChild(textElement(doc, TagName, to[i]));
                    parent.appendChild(next);
                }
                parent = next;
            }
            QDomElement existing = childMenu(parent, to.last());
            if (existing.isNull()) {
                QDomElement name = source.firstChildElement(TagName);
                setText(name, to.last());
                parent.appendChild(source);
            } else {
                moveContents(source, existing);
            }
        }
        for (const QDomElement &child : childElements(menu, TagMenu))
            applyMoves(child);
    }

    bool matches(const QDomElement &rule, const DesktopEntry &entry);

    bool anyMatches(const QDomElement &rules, const DesktopEntry &entry)
    {
        for (QDomElement rule = rules.firstChildElement(); !rule.isNull();
            rule = rule.nextSiblingElement()) {
            if (matches(rule, entry))
                return true;
        }
        return false;
    }

    bool matches(const QDomElement &rule, const DesktopEntry &entry)
    {
        const QString tag = rule.tagName();
        if (tag == QLatin1String("Filename"))
            return entry.id == text(rule);
        if (tag == QLatin1String("Category"))
            return entry.categories.contains(text(rule));
        if (tag == QLatin1String("All"))
            return true;
        if (tag == QLatin1String("Or"))
            return anyMatches(rule, entry);
        if (tag == QLatin1String("Not"))
            return !anyMatches(rule, entry);
        if (tag == QLatin1String("And")) {
            bool any = false;
            for (QDomElement part = rule.firstChildElement(); !part.isNull();
                part = part.nextSiblingElement()) {
                if (!matches(part, entry))
                    return false;
                any = true;
            }
            return any;
        }
        return false;
    }

    // A menu with its settings worked out and its applications chosen.
    struct Node
    {
        QDomElement element;
        QString key; // <Name>
        QString displayName;
        bool hidden = false;
        bool onlyUnallocated = false;
        QStringList appDirs; // least important first
        QDomElement layout;
        QMap<QString, DesktopEntry> entries; // by desktop-file ID
        QList<Node> submenus;
    };

    using Pool = QHash<QString, DesktopEntry>;

    class Builder
    {
    public:
        Builder(const QString &desktopName, const QString &localeName, XdgMenu *out)
            : m_desktopName(desktopName)
            , m_locale(localeName)
            , m_out(out)
        {
        }

        Node build(const QDomElement &menu, const QStringList &parentAppDirs,
            const QStringList &parentDirectoryDirs, const QDomElement &parentDefaultLayout)
        {
            Node node;
            node.element = menu;
            node.key = childText(menu, TagName);
            node.appDirs = parentAppDirs;
            QStringList directoryDirs = parentDirectoryDirs;
            QStringList directories;
            QDomElement defaultLayout = parentDefaultLayout;
            for (QDomElement child = menu.firstChildElement(); !child.isNull();
                child = child.nextSiblingElement()) {
                const QString tag = child.tagName();
                if (tag == QLatin1String("AppDir"))
                    node.appDirs << text(child);
                else if (tag == QLatin1String("DirectoryDir"))
                    directoryDirs << text(child);
                else if (tag == QLatin1String("Directory"))
                    directories << text(child);
                else if (tag == QLatin1String("OnlyUnallocated"))
                    node.onlyUnallocated = true;
                else if (tag == QLatin1String("NotOnlyUnallocated"))
                    node.onlyUnallocated = false;
                else if (tag == QLatin1String("Deleted"))
                    node.hidden = true;
                else if (tag == QLatin1String("NotDeleted"))
                    node.hidden = false;
                else if (tag == QLatin1String("Layout"))
                    node.layout = child;
                else if (tag == QLatin1String("DefaultLayout"))
                    defaultLayout = child;
            }
            if (node.layout.isNull())
                node.layout = defaultLayout;

            // A deleted or NoDisplay folder is not shown, but still takes its
            // applications, so hiding a folder does not move them elsewhere.
            node.displayName = node.key;
            for (auto dir = directories.crbegin(); dir != directories.crend(); ++dir) {
                if (readDirectory(*dir, directoryDirs, &node))
                    break;
            }
            for (const QString &dir : std::as_const(directoryDirs)) {
                if (QFileInfo(dir).isDir())
                    m_out->watchDirs.append(dir);
            }

            if (!node.onlyUnallocated) {
                choose(&node, false);
                for (auto it = node.entries.cbegin(); it != node.entries.cend(); ++it)
                    m_allocated.insert(it.key());
            }
            for (const QDomElement &child : childElements(menu, TagMenu)) {
                Node submenu = build(child, node.appDirs, directoryDirs, defaultLayout);
                if (!submenu.hidden)
                    node.submenus.append(submenu);
            }
            return node;
        }

        // The second pass: menus with <OnlyUnallocated/> get what no other
        // menu took.
        void fillUnallocated(Node *node)
        {
            if (node->onlyUnallocated)
                choose(node, true);
            for (Node &submenu : node->submenus)
                fillUnallocated(&submenu);
        }

    private:
        bool readDirectory(const QString &name, const QStringList &dirs, Node *node)
        {
            for (auto dir = dirs.crbegin(); dir != dirs.crend(); ++dir) {
                const QString path = absolutePath(*dir, name);
                if (!QFileInfo(path).isFile())
                    continue;
                const std::optional<DesktopEntry> directory
                    = DesktopEntry::parse(path, name, m_locale);
                if (!directory)
                    continue;
                m_out->watchFiles.append(path);
                if (!directory->name.isEmpty())
                    node->displayName = directory->name;
                node->hidden = node->hidden || directory->noDisplay || directory->hidden;
                return true;
            }
            return false;
        }

        void choose(Node *node, bool unallocatedOnly)
        {
            const Pool &pool = poolFor(node->appDirs);
            for (QDomElement rule = node->element.firstChildElement(); !rule.isNull();
                rule = rule.nextSiblingElement()) {
                const bool include = rule.tagName() == QLatin1String("Include");
                if (!include && rule.tagName() != QLatin1String("Exclude"))
                    continue;
                for (auto it = pool.cbegin(); it != pool.cend(); ++it) {
                    if (unallocatedOnly && include && m_allocated.contains(it.key()))
                        continue;
                    if (!anyMatches(rule, *it))
                        continue;
                    if (include)
                        node->entries.insert(it.key(), *it);
                    else
                        node->entries.remove(it.key());
                }
            }
        }

        // The applications in dirs, by desktop-file ID: a later directory's
        // file replaces an earlier one's with the same ID, even when it is
        // hidden. Only those the Start menu shows are kept.
        const Pool &poolFor(const QStringList &dirs)
        {
            const QString key = dirs.join(QLatin1Char('\n'));
            auto cached = m_pools.constFind(key);
            if (cached != m_pools.cend())
                return *cached;
            QHash<QString, DesktopEntry> all;
            for (const QString &dir : dirs) {
                const QHash<QString, DesktopEntry> &inDir = entriesIn(dir);
                for (auto it = inDir.cbegin(); it != inDir.cend(); ++it)
                    all.insert(it.key(), *it);
            }
            Pool pool;
            for (auto it = all.cbegin(); it != all.cend(); ++it) {
                if (it->shouldShow(m_desktopName))
                    pool.insert(it.key(), *it);
            }
            return *m_pools.insert(key, pool);
        }

        const QHash<QString, DesktopEntry> &entriesIn(const QString &dirPath)
        {
            auto cached = m_dirs.constFind(dirPath);
            if (cached != m_dirs.cend())
                return *cached;
            QHash<QString, DesktopEntry> entries;
            const QDir dir(dirPath);
            if (dir.exists()) {
                m_out->watchDirs.append(dirPath);
                QDirIterator subdirs(
                    dirPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                while (subdirs.hasNext())
                    m_out->watchDirs.append(subdirs.next());
                QDirIterator it(dirPath, { QStringLiteral("*.desktop") }, QDir::Files,
                    QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString file = it.next();
                    // The desktop-file ID: the path below the directory,
                    // with '/' replaced by '-'.
                    QString id = dir.relativeFilePath(file);
                    id.replace(QLatin1Char('/'), QLatin1Char('-'));
                    std::optional<DesktopEntry> entry = DesktopEntry::parse(file, id, m_locale);
                    if (entry)
                        entries.insert(id, *entry);
                }
            }
            return *m_dirs.insert(dirPath, entries);
        }

        QString m_desktopName;
        QString m_locale;
        XdgMenu *m_out;
        QSet<QString> m_allocated;
        QHash<QString, QHash<QString, DesktopEntry>> m_dirs;
        QHash<QString, Pool> m_pools;
    };

    bool byName(const MenuNode &a, const MenuNode &b)
    {
        const QString &nameA = a.kind == MenuNode::Kind::Entry ? a.entry.name : a.name;
        const QString &nameB = b.kind == MenuNode::Kind::Entry ? b.entry.name : b.name;
        return QString::localeAwareCompare(nameA, nameB) < 0;
    }

    bool hasEntries(const MenuNode &node)
    {
        return std::any_of(node.children.cbegin(), node.children.cend(), [](const MenuNode &n) {
            return n.kind == MenuNode::Kind::Entry
                || (n.kind == MenuNode::Kind::Folder && hasEntries(n));
        });
    }

    // Drops separators at either end and repeated ones.
    void tidySeparators(QList<MenuNode> *items)
    {
        QList<MenuNode> out;
        for (const MenuNode &item : std::as_const(*items)) {
            if (item.kind == MenuNode::Kind::Separator
                && (out.isEmpty() || out.last().kind == MenuNode::Kind::Separator))
                continue;
            out.append(item);
        }
        while (!out.isEmpty() && out.last().kind == MenuNode::Kind::Separator)
            out.removeLast();
        *items = out;
    }

    MenuNode toMenuNode(const Node &node)
    {
        MenuNode out;
        out.name = node.displayName;

        // Folders by <Name>, then entries, each sorted by display name.
        QList<QPair<QString, MenuNode>> folders;
        for (const Node &submenu : node.submenus) {
            MenuNode folder = toMenuNode(submenu);
            if (hasEntries(folder))
                folders.append({ submenu.key, folder });
        }
        std::stable_sort(folders.begin(), folders.end(),
            [](const auto &a, const auto &b) { return byName(a.second, b.second); });
        QList<MenuNode> entries;
        for (const DesktopEntry &entry : node.entries) {
            MenuNode item;
            item.kind = MenuNode::Kind::Entry;
            item.entry = entry;
            entries.append(item);
        }
        std::stable_sort(entries.begin(), entries.end(), byName);

        if (node.layout.isNull()) {
            for (const auto &folder : std::as_const(folders))
                out.children.append(folder.second);
            out.children.append(entries);
            return out;
        }

        // <Merge> places what the layout does not name.
        QSet<QString> namedMenus;
        QSet<QString> namedFiles;
        for (QDomElement item = node.layout.firstChildElement(); !item.isNull();
            item = item.nextSiblingElement()) {
            if (item.tagName() == QLatin1String("Menuname"))
                namedMenus.insert(text(item));
            else if (item.tagName() == QLatin1String("Filename"))
                namedFiles.insert(text(item));
        }
        QSet<QString> placedMenus;
        QSet<QString> placedFiles;
        auto unplacedFolders = [&] {
            QList<MenuNode> list;
            for (const auto &folder : std::as_const(folders)) {
                if (!namedMenus.contains(folder.first) && !placedMenus.contains(folder.first)) {
                    placedMenus.insert(folder.first);
                    list.append(folder.second);
                }
            }
            return list;
        };
        auto unplacedEntries = [&] {
            QList<MenuNode> list;
            for (const MenuNode &entry : std::as_const(entries)) {
                if (!namedFiles.contains(entry.entry.id) && !placedFiles.contains(entry.entry.id)) {
                    placedFiles.insert(entry.entry.id);
                    list.append(entry);
                }
            }
            return list;
        };
        for (QDomElement item = node.layout.firstChildElement(); !item.isNull();
            item = item.nextSiblingElement()) {
            const QString tag = item.tagName();
            if (tag == QLatin1String("Menuname")) {
                for (const auto &folder : std::as_const(folders)) {
                    if (folder.first == text(item) && !placedMenus.contains(folder.first)) {
                        placedMenus.insert(folder.first);
                        out.children.append(folder.second);
                    }
                }
            } else if (tag == QLatin1String("Filename")) {
                for (const MenuNode &entry : std::as_const(entries)) {
                    if (entry.entry.id == text(item) && !placedFiles.contains(entry.entry.id)) {
                        placedFiles.insert(entry.entry.id);
                        out.children.append(entry);
                    }
                }
            } else if (tag == QLatin1String("Separator")) {
                MenuNode separator;
                separator.kind = MenuNode::Kind::Separator;
                out.children.append(separator);
            } else if (tag == QLatin1String("Merge")) {
                const QString type = item.attribute(QStringLiteral("type"));
                if (type == QLatin1String("menus")) {
                    out.children.append(unplacedFolders());
                } else if (type == QLatin1String("files")) {
                    out.children.append(unplacedEntries());
                } else if (type == QLatin1String("all")) {
                    QList<MenuNode> both = unplacedFolders() + unplacedEntries();
                    std::stable_sort(both.begin(), both.end(), byName);
                    out.children.append(both);
                }
            }
        }
        tidySeparators(&out.children);
        return out;
    }

    void collectEntries(const MenuNode &node, QHash<QString, DesktopEntry> *out)
    {
        for (const MenuNode &child : node.children) {
            if (child.kind == MenuNode::Kind::Entry)
                out->insert(child.entry.id, child.entry);
            else if (child.kind == MenuNode::Kind::Folder)
                collectEntries(child, out);
        }
    }

} // namespace

XdgMenuPaths XdgMenuPaths::fromEnvironment()
{
    XdgMenuPaths paths;
    for (const QString &dir :
        QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation))
        paths.configDirs << QDir::cleanPath(dir);
    for (const QString &dir :
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        paths.dataDirs << QDir::cleanPath(dir);
    return paths;
}

QString findApplicationsMenu(const XdgMenuPaths &paths, const QString &prefix)
{
    QStringList names;
    if (!prefix.isEmpty() && !prefix.contains(QLatin1Char('/')))
        names << prefix + QStringLiteral("applications.menu");
    names << QStringLiteral("applications.menu");
    for (const QString &name : std::as_const(names)) {
        for (const QString &dir : paths.configDirs) {
            const QString path = dir + QStringLiteral("/menus/") + name;
            if (QFileInfo(path).isFile())
                return path;
        }
    }
    return QString::fromLatin1(BuiltInMenuFile);
}

XdgMenu loadXdgMenu(const QString &menuFile, const XdgMenuPaths &paths, const QString &desktopName,
    const QString &localeName)
{
    XdgMenu out;
    // A user's menu file appearing (a menu editor's first change) or
    // changing, and merge files being added.
    for (const QString &dir : paths.configDirs) {
        const QString menus = dir + QStringLiteral("/menus");
        if (QFileInfo(menus).isDir())
            out.watchDirs << menus;
    }
    if (!paths.configDirs.isEmpty()
        && !QFileInfo(paths.configDirs.first() + QStringLiteral("/menus")).isDir()
        && QFileInfo(paths.configDirs.first()).isDir())
        out.watchDirs << paths.configDirs.first();

    QDomDocument doc;
    QDomElement root = Loader(paths, &out).load(menuFile, doc);
    if (root.isNull()) {
        qCWarning(lcMenu) << "no usable menu in" << menuFile;
    } else {
        doc.appendChild(root);
        consolidate(root);
        applyMoves(root);
        consolidate(root);
        Builder builder(desktopName, localeName, &out);
        Node top = builder.build(root, {}, {}, {});
        builder.fillUnallocated(&top);
        out.root = toMenuNode(top);
    }
    out.root.kind = MenuNode::Kind::Folder;
    out.watchFiles.removeDuplicates();
    out.watchDirs.removeDuplicates();
    return out;
}

QList<DesktopEntry> flattenMenu(const MenuNode &root)
{
    QHash<QString, DesktopEntry> byId;
    collectEntries(root, &byId);
    QList<DesktopEntry> entries = byId.values();
    std::sort(entries.begin(), entries.end(), [](const DesktopEntry &a, const DesktopEntry &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return entries;
}

} // namespace Daytona
