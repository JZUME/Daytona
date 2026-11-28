// SPDX-License-Identifier: GPL-3.0-or-later

#include "xembedproxy.h"

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QtEndian>
#include <cstdlib>
#include <memory>
#include <xcb/composite.h>
#include <xcb/damage.h>

Q_LOGGING_CATEGORY(lcXEmbed, "daytona.xembed", QtWarningMsg)

namespace Daytona {

namespace {
    const QString WatcherService = QStringLiteral("org.kde.StatusNotifierWatcher");
    const QString WatcherPath = QStringLiteral("/StatusNotifierWatcher");
    const QString ItemPath = QStringLiteral("/StatusNotifierItem");

    // System tray protocol opcodes, and XEmbed's.
    constexpr uint32_t SystemTrayRequestDock = 0;
    constexpr uint32_t XEmbedEmbeddedNotify = 0;
    constexpr uint32_t XEmbedMapped = 1 << 0;

    // Containers wait here, outside every screen, while not clicked.
    constexpr int HiddenPosition = -1000;

    template <typename T> using Reply = std::unique_ptr<T, decltype(&std::free)>;

    template <typename T> Reply<T> reply(T *r)
    {
        return Reply<T>(r, &std::free);
    }
} // namespace

QDBusArgument &operator<<(QDBusArgument &argument, const SniPixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SniPixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

// XEmbedIcon

XEmbedIcon::XEmbedIcon(XEmbedProxy *proxy, xcb_window_t client)
    : QObject(proxy)
    , m_proxy(proxy)
    , m_client(client)
    , m_connectionName(QStringLiteral("daytona-xembed-%1").arg(client))
{
    // Programs often redraw their icon in several steps.
    m_iconTimer.setSingleShot(true);
    m_iconTimer.setInterval(50);
    connect(&m_iconTimer, &QTimer::timeout, this, &XEmbedIcon::updateIcon);

    embed();
    if (!isEmbedded())
        return;
    readTitle();

    QDBusConnection bus
        = QDBusConnection::connectToBus(QDBusConnection::SessionBus, m_connectionName);
    if (!bus.isConnected()
        || !bus.registerObject(ItemPath, this,
            QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableSlots
                | QDBusConnection::ExportScriptableSignals)) {
        qCWarning(lcXEmbed) << "cannot export a tray icon on the session bus";
        return;
    }
    registerItem();
}

XEmbedIcon::~XEmbedIcon()
{
    // Closing the connection takes the item out of the notification area.
    QDBusConnection::disconnectFromBus(m_connectionName);
    xcb_connection_t *c = m_proxy->connection();
    if (m_damage)
        xcb_damage_destroy(c, m_damage);
    if (m_container != XCB_WINDOW_NONE) {
        // An icon still in the container (when the proxy ends) goes back to
        // the root window first, as destroying the container would destroy
        // it with it. (For an icon already gone, this fails harmlessly.)
        xcb_unmap_window(c, m_client);
        xcb_reparent_window(c, m_client, m_proxy->root(), 0, 0);
        xcb_destroy_window(c, m_container);
    }
    xcb_flush(c);
}

void XEmbedIcon::embed()
{
    xcb_connection_t *c = m_proxy->connection();

    auto geometry = reply(xcb_get_geometry_reply(c, xcb_get_geometry(c, m_client), nullptr));
    if (!geometry)
        return; // already gone

    const uint32_t events[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(c, m_client, XCB_CW_EVENT_MASK, events);
    // If the proxy ends, the X server gives icons back to the root window
    // instead of destroying them under their programs.
    xcb_change_save_set(c, XCB_SET_MODE_INSERT, m_client);

    // A 32-bit container keeps the icon's transparency; a client with a
    // parent-relative background cannot change depth, so it gets one of
    // its parent's depth instead.
    m_container = m_proxy->createContainer();
    auto error
        = reply(xcb_request_check(c, xcb_reparent_window_checked(c, m_client, m_container, 0, 0)));
    if (error) {
        xcb_destroy_window(c, m_container);
        m_container = xcb_generate_id(c);
        const uint32_t values[] = { 1, XCB_EVENT_MASK_NO_EVENT };
        xcb_create_window(c, XCB_COPY_FROM_PARENT, m_container, m_proxy->root(), HiddenPosition,
            HiddenPosition, XEmbedProxy::IconSize, XEmbedProxy::IconSize, 0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
            XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, values);
        xcb_map_window(c, m_container);
        error = reply(
            xcb_request_check(c, xcb_reparent_window_checked(c, m_client, m_container, 0, 0)));
        if (error) {
            qCWarning(lcXEmbed) << "cannot embed tray icon window" << m_client;
            xcb_destroy_window(c, m_container);
            m_container = XCB_WINDOW_NONE;
            return;
        }
    }

    // The icon is read from its own pixmap, not drawn into the container.
    xcb_composite_redirect_window(c, m_client, XCB_COMPOSITE_REDIRECT_MANUAL);
    const uint32_t size[] = { 0, 0, XEmbedProxy::IconSize, XEmbedProxy::IconSize, 0 };
    xcb_configure_window(c, m_client,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
            | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
        size);

    xcb_client_message_event_t notify = {};
    notify.response_type = XCB_CLIENT_MESSAGE;
    notify.format = 32;
    notify.window = m_client;
    notify.type = m_proxy->atom("_XEMBED");
    notify.data.data32[0] = XCB_CURRENT_TIME;
    notify.data.data32[1] = XEmbedEmbeddedNotify;
    notify.data.data32[3] = m_container;
    notify.data.data32[4] = 0; // XEmbed protocol version
    xcb_send_event(c, false, m_client, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<char *>(&notify));

    m_damage = xcb_generate_id(c);
    xcb_damage_create(c, m_damage, m_client, XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    propertyChanged(m_proxy->atom("_XEMBED_INFO"));
    xcb_flush(c);
}

void XEmbedIcon::readTitle()
{
    xcb_connection_t *c = m_proxy->connection();
    auto property = [&](xcb_atom_t name, xcb_atom_t type) {
        auto r = reply(xcb_get_property_reply(
            c, xcb_get_property(c, false, m_client, name, type, 0, 1024), nullptr));
        if (!r || r->format != 8)
            return QByteArray();
        return QByteArray(static_cast<const char *>(xcb_get_property_value(r.get())),
            xcb_get_property_value_length(r.get()));
    };

    QString title
        = QString::fromUtf8(property(m_proxy->atom("_NET_WM_NAME"), m_proxy->atom("UTF8_STRING")));
    if (title.isEmpty())
        title = QString::fromLatin1(property(XCB_ATOM_WM_NAME, XCB_ATOM_STRING));
    // WM_CLASS is "instance\0class\0".
    const QList<QByteArray> wmClass = property(XCB_ATOM_WM_CLASS, XCB_ATOM_STRING).split('\0');
    QString id = wmClass.size() > 1 ? QString::fromLatin1(wmClass.at(1)) : QString();
    if (id.isEmpty())
        id = QStringLiteral("xembed-%1").arg(m_client);
    // Programs without a title of their own are known by their class.
    if (title.isEmpty())
        title = wmClass.size() > 1 ? QString::fromLatin1(wmClass.at(1)) : QString();
    m_id = id;
    if (title != m_title) {
        m_title = title;
        Q_EMIT NewTitle();
    }
}

void XEmbedIcon::registerItem()
{
    QDBusConnection bus(m_connectionName);
    if (!bus.isConnected())
        return;
    QDBusMessage message = QDBusMessage::createMethodCall(WatcherService, WatcherPath,
        QStringLiteral("org.kde.StatusNotifierWatcher"),
        QStringLiteral("RegisterStatusNotifierItem"));
    message << bus.baseService();
    bus.asyncCall(message);
}

void XEmbedIcon::damaged()
{
    if (!m_iconTimer.isActive())
        m_iconTimer.start();
}

void XEmbedIcon::propertyChanged(xcb_atom_t atom)
{
    xcb_connection_t *c = m_proxy->connection();
    if (atom == m_proxy->atom("_XEMBED_INFO")) {
        // [version, flags]; without the property the icon is shown.
        auto info = reply(xcb_get_property_reply(
            c, xcb_get_property(c, false, m_client, atom, atom, 0, 2), nullptr));
        bool mapped = true;
        if (info && info->format == 32 && xcb_get_property_value_length(info.get()) >= 8)
            mapped = static_cast<const uint32_t *>(xcb_get_property_value(info.get()))[1]
                & XEmbedMapped;
        if (mapped)
            xcb_map_window(c, m_client);
        else
            xcb_unmap_window(c, m_client);
        xcb_flush(c);
    } else if (atom == XCB_ATOM_WM_NAME || atom == m_proxy->atom("_NET_WM_NAME")) {
        readTitle();
    }
}

void XEmbedIcon::configured(int width, int height)
{
    // The tray decides the icon's size.
    if (width == XEmbedProxy::IconSize && height == XEmbedProxy::IconSize)
        return;
    const uint32_t size[] = { XEmbedProxy::IconSize, XEmbedProxy::IconSize };
    xcb_configure_window(
        m_proxy->connection(), m_client, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, size);
    xcb_flush(m_proxy->connection());
}

void XEmbedIcon::setMapped(bool mapped)
{
    if (mapped == m_mapped)
        return;
    m_mapped = mapped;
    if (mapped)
        damaged();
    Q_EMIT NewStatus(status());
}

void XEmbedIcon::updateIcon()
{
    xcb_connection_t *c = m_proxy->connection();
    auto geometry = reply(xcb_get_geometry_reply(c, xcb_get_geometry(c, m_client), nullptr));
    if (!geometry || !m_mapped)
        return;
    const int width = geometry->width;
    const int height = geometry->height;
    auto image = reply(xcb_get_image_reply(c,
        xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, m_client, 0, 0, width, height, ~0u), nullptr));
    // Only 32 bits per pixel, the X server's layout for depths 24 and 32,
    // in the byte order of every architecture Daytona targets.
    if (!image || xcb_get_image_data_length(image.get()) != width * height * 4
        || xcb_get_setup(c)->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST) {
        return;
    }
    // X11's 32-bit pixels are premultiplied 0xAARRGGBB (no alpha at depth
    // 24); the item's are straight ARGB in network byte order.
    const bool alpha = geometry->depth == 32;
    const auto *in = reinterpret_cast<const quint32 *>(xcb_get_image_data(image.get()));
    SniPixmap pixmap { width, height, {} };
    pixmap.data.resize(qsizetype(width) * height * 4);
    auto *out = reinterpret_cast<uchar *>(pixmap.data.data());
    bool blank = true;
    for (qsizetype i = 0; i < qsizetype(width) * height; ++i) {
        quint32 pixel = in[i];
        if (!alpha) {
            pixel |= 0xff000000u;
        } else if (const quint32 a = pixel >> 24; a == 0) {
            pixel = 0;
        } else if (a < 255) {
            auto channel = [&](int shift) {
                return qMin(255u, ((pixel >> shift) & 0xff) * 255 / a) << shift;
            };
            pixel = (a << 24) | channel(16) | channel(8) | channel(0);
        }
        blank = blank && (pixel >> 24) == 0;
        qToBigEndian<quint32>(pixel, out + i * 4);
    }
    // A fully transparent icon has not been drawn yet; keep the last one.
    if (blank)
        return;
    m_pixmaps = { pixmap };
    Q_EMIT NewIcon();
}

void XEmbedIcon::Activate(int x, int y)
{
    m_proxy->click(m_container, m_client, XCB_BUTTON_INDEX_1, { x, y });
}

void XEmbedIcon::SecondaryActivate(int x, int y)
{
    m_proxy->click(m_container, m_client, XCB_BUTTON_INDEX_2, { x, y });
}

void XEmbedIcon::ContextMenu(int x, int y)
{
    m_proxy->click(m_container, m_client, XCB_BUTTON_INDEX_3, { x, y });
}

void XEmbedIcon::Scroll(int delta, const QString &orientation)
{
    if (delta == 0)
        return;
    // X11's wheel buttons: 4 up, 5 down, 6 left, 7 right.
    const bool horizontal
        = orientation.compare(QLatin1String("horizontal"), Qt::CaseInsensitive) == 0;
    const uint8_t button = horizontal ? (delta > 0 ? 7 : 6) : (delta > 0 ? 4 : 5);
    xcb_connection_t *c = m_proxy->connection();
    auto pointer
        = reply(xcb_query_pointer_reply(c, xcb_query_pointer(c, m_proxy->root()), nullptr));
    if (pointer)
        m_proxy->click(m_container, m_client, button, { pointer->root_x, pointer->root_y });
}

// XEmbedProxy

XEmbedProxy::XEmbedProxy(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<SniPixmap>();
    qDBusRegisterMetaType<QList<SniPixmap>>();
}

XEmbedProxy::~XEmbedProxy()
{
    qDeleteAll(m_icons);
    m_icons.clear();
    if (m_connection)
        xcb_disconnect(m_connection);
}

xcb_atom_t XEmbedProxy::atom(const char *name)
{
    const QByteArray key(name);
    auto it = m_atoms.constFind(key);
    if (it != m_atoms.constEnd())
        return *it;
    auto r = reply(xcb_intern_atom_reply(
        m_connection, xcb_intern_atom(m_connection, false, key.size(), name), nullptr));
    const xcb_atom_t value = r ? r->atom : xcb_atom_t(XCB_ATOM_NONE);
    m_atoms.insert(key, value);
    return value;
}

void XEmbedProxy::findVisual()
{
    for (auto depths = xcb_screen_allowed_depths_iterator(m_screen); depths.rem;
        xcb_depth_next(&depths)) {
        if (depths.data->depth != 32)
            continue;
        for (auto visuals = xcb_depth_visuals_iterator(depths.data); visuals.rem;
            xcb_visualtype_next(&visuals)) {
            if (visuals.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
                m_visual = visuals.data->visual_id;
                m_depth = 32;
                m_colormap = xcb_generate_id(m_connection);
                xcb_create_colormap(
                    m_connection, XCB_COLORMAP_ALLOC_NONE, m_colormap, m_screen->root, m_visual);
                return;
            }
        }
    }
    m_visual = m_screen->root_visual;
    m_depth = m_screen->root_depth;
}

bool XEmbedProxy::start()
{
    int screenNumber = 0;
    m_connection = xcb_connect(nullptr, &screenNumber);
    if (xcb_connection_has_error(m_connection)) {
        qCWarning(lcXEmbed) << "cannot connect to the X display";
        return false;
    }
    auto screens = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
    for (int i = 0; i < screenNumber && screens.rem; ++i)
        xcb_screen_next(&screens);
    m_screen = screens.data;

    // Composite keeps the icons' contents readable while out of sight,
    // and Damage says when they change.
    auto composite = reply(xcb_composite_query_version_reply(
        m_connection, xcb_composite_query_version(m_connection, 0, 4), nullptr));
    auto damage = reply(xcb_damage_query_version_reply(
        m_connection, xcb_damage_query_version(m_connection, 1, 1), nullptr));
    const xcb_query_extension_reply_t *damageExtension
        = xcb_get_extension_data(m_connection, &xcb_damage_id);
    if (!composite || !damage || !damageExtension || !damageExtension->present) {
        qCWarning(lcXEmbed) << "the X server lacks Composite or Damage";
        return false;
    }
    m_damageEvent = damageExtension->first_event;

    findVisual();

    const QByteArray selectionName = "_NET_SYSTEM_TRAY_S" + QByteArray::number(screenNumber);
    m_selection = atom(selectionName.constData());
    auto owner = reply(xcb_get_selection_owner_reply(
        m_connection, xcb_get_selection_owner(m_connection, m_selection), nullptr));
    if (!owner || owner->owner != XCB_WINDOW_NONE) {
        qCWarning(lcXEmbed) << "another program is the X11 system tray already";
        return false;
    }

    m_selectionOwner = xcb_generate_id(m_connection);
    const uint32_t ownerValues[] = { 1 };
    xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, m_selectionOwner, m_screen->root, -1, -1,
        1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_OVERRIDE_REDIRECT,
        ownerValues);
    const uint32_t horizontal = 0;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_selectionOwner,
        atom("_NET_SYSTEM_TRAY_ORIENTATION"), XCB_ATOM_CARDINAL, 32, 1, &horizontal);
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_selectionOwner,
        atom("_NET_SYSTEM_TRAY_VISUAL"), XCB_ATOM_VISUALID, 32, 1, &m_visual);
    const uint32_t iconSize = IconSize;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_selectionOwner,
        atom("_NET_SYSTEM_TRAY_ICON_SIZE"), XCB_ATOM_CARDINAL, 32, 1, &iconSize);
    const uint32_t padding = 0;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_selectionOwner,
        atom("_NET_SYSTEM_TRAY_PADDING"), XCB_ATOM_CARDINAL, 32, 1, &padding);

    // The X server's time now: the time of a change to a property.
    const uint32_t propertyEvents[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes(m_connection, m_selectionOwner, XCB_CW_EVENT_MASK, propertyEvents);
    xcb_change_property(m_connection, XCB_PROP_MODE_APPEND, m_selectionOwner,
        atom("_NET_SYSTEM_TRAY_PADDING"), XCB_ATOM_CARDINAL, 32, 0, nullptr);
    xcb_flush(m_connection);
    while (xcb_generic_event_t *event = xcb_wait_for_event(m_connection)) {
        const bool found = (event->response_type & ~0x80) == XCB_PROPERTY_NOTIFY;
        if (found)
            m_serverTimeBase = reinterpret_cast<xcb_property_notify_event_t *>(event)->time;
        std::free(event);
        if (found)
            break;
    }
    m_serverTimer.start();
    const uint32_t noEvents[] = { XCB_EVENT_MASK_NO_EVENT };
    xcb_change_window_attributes(m_connection, m_selectionOwner, XCB_CW_EVENT_MASK, noEvents);

    // The root window's size, for clicks, as the screens change.
    // Substructure too, to see the menus programs open.
    const uint32_t rootEvents[]
        = { XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY };
    xcb_change_window_attributes(m_connection, m_screen->root, XCB_CW_EVENT_MASK, rootEvents);
    m_rootSize = QSize(m_screen->width_in_pixels, m_screen->height_in_pixels);
    if (auto root = reply(xcb_get_geometry_reply(
            m_connection, xcb_get_geometry(m_connection, m_screen->root), nullptr))) {
        m_rootSize = QSize(root->width, root->height);
    }

    xcb_set_selection_owner(m_connection, m_selectionOwner, m_selection, XCB_CURRENT_TIME);
    owner = reply(xcb_get_selection_owner_reply(
        m_connection, xcb_get_selection_owner(m_connection, m_selection), nullptr));
    if (!owner || owner->owner != m_selectionOwner) {
        qCWarning(lcXEmbed) << "cannot become the X11 system tray";
        return false;
    }

    // Tell programs waiting for a tray that there is one now.
    xcb_client_message_event_t manager = {};
    manager.response_type = XCB_CLIENT_MESSAGE;
    manager.format = 32;
    manager.window = m_screen->root;
    manager.type = atom("MANAGER");
    manager.data.data32[0] = XCB_CURRENT_TIME;
    manager.data.data32[1] = m_selection;
    manager.data.data32[2] = m_selectionOwner;
    xcb_send_event(m_connection, false, m_screen->root, XCB_EVENT_MASK_STRUCTURE_NOTIFY,
        reinterpret_cast<char *>(&manager));
    xcb_flush(m_connection);

    auto *notifier
        = new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &XEmbedProxy::processEvents);
    // Replies can bring events in with them; handle those before sleeping.
    connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock, this,
        &XEmbedProxy::processEvents);

    // A restarted shell has a new watcher, which knows none of the icons.
    auto *watcher = new QDBusServiceWatcher(WatcherService, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        for (XEmbedIcon *icon : std::as_const(m_icons))
            icon->registerItem();
    });
    return true;
}

void XEmbedProxy::processEvents()
{
    while (xcb_generic_event_t *event = xcb_poll_for_event(m_connection)) {
        handleEvent(event);
        std::free(event);
    }
    if (xcb_connection_has_error(m_connection)) {
        // Xwayland has gone, and every X11 program with it.
        qCDebug(lcXEmbed) << "the X display has closed";
        QCoreApplication::exit(0);
    }
}

XEmbedIcon *XEmbedProxy::iconFor(xcb_window_t window) const
{
    return m_icons.value(window);
}

void XEmbedProxy::handleEvent(xcb_generic_event_t *event)
{
    const uint8_t type = event->response_type & ~0x80;
    if (type == 0)
        return; // an error of a request nobody checks; nothing to do
    if (type == m_damageEvent + XCB_DAMAGE_NOTIFY) {
        auto *damage = reinterpret_cast<xcb_damage_notify_event_t *>(event);
        xcb_damage_subtract(m_connection, damage->damage, XCB_NONE, XCB_NONE);
        if (XEmbedIcon *icon = iconFor(damage->drawable))
            icon->damaged();
        return;
    }
    switch (type) {
    case XCB_CLIENT_MESSAGE: {
        auto *message = reinterpret_cast<xcb_client_message_event_t *>(event);
        if (message->window == m_selectionOwner && message->format == 32
            && message->type == atom("_NET_SYSTEM_TRAY_OPCODE")
            && message->data.data32[1] == SystemTrayRequestDock) {
            dock(message->data.data32[2]);
        }
        break;
    }
    case XCB_DESTROY_NOTIFY: {
        const xcb_window_t window = reinterpret_cast<xcb_destroy_notify_event_t *>(event)->window;
        if (window == m_menu)
            hideCatcher();
        undock(window);
        break;
    }
    case XCB_REPARENT_NOTIFY: {
        auto *reparent = reinterpret_cast<xcb_reparent_notify_event_t *>(event);
        XEmbedIcon *icon = iconFor(reparent->window);
        if (icon && reparent->parent != icon->container())
            undock(reparent->window); // the program took its icon back
        break;
    }
    case XCB_MAP_NOTIFY: {
        auto *map = reinterpret_cast<xcb_map_notify_event_t *>(event);
        if (XEmbedIcon *icon = iconFor(map->window))
            icon->setMapped(true);
        else if (map->event == m_screen->root && map->override_redirect)
            popupMapped(map->window);
        break;
    }
    case XCB_UNMAP_NOTIFY: {
        const xcb_window_t window = reinterpret_cast<xcb_unmap_notify_event_t *>(event)->window;
        if (XEmbedIcon *icon = iconFor(window))
            icon->setMapped(false);
        else if (window == m_menu)
            hideCatcher();
        break;
    }
    case XCB_BUTTON_PRESS:
        // A click beside a menu that did not take it (one that grabs the
        // pointer, as GTK's and Wine's do, closes itself).
        if (reinterpret_cast<xcb_button_press_event_t *>(event)->event == m_catcher)
            hideCatcher();
        break;
    case XCB_CONFIGURE_NOTIFY: {
        auto *configure = reinterpret_cast<xcb_configure_notify_event_t *>(event);
        if (configure->window == m_screen->root)
            m_rootSize = QSize(configure->width, configure->height);
        else if (XEmbedIcon *icon = iconFor(configure->window))
            icon->configured(configure->width, configure->height);
        break;
    }
    case XCB_PROPERTY_NOTIFY: {
        auto *property = reinterpret_cast<xcb_property_notify_event_t *>(event);
        if (XEmbedIcon *icon = iconFor(property->window))
            icon->propertyChanged(property->atom);
        break;
    }
    case XCB_SELECTION_CLEAR:
        if (reinterpret_cast<xcb_selection_clear_event_t *>(event)->owner == m_selectionOwner) {
            qCWarning(lcXEmbed) << "another program has taken over the X11 system tray";
            QCoreApplication::exit(0);
        }
        break;
    default:
        break;
    }
}

void XEmbedProxy::dock(xcb_window_t client)
{
    if (client == XCB_WINDOW_NONE || m_icons.contains(client))
        return;
    auto *icon = new XEmbedIcon(this, client);
    if (!icon->isEmbedded()) {
        delete icon;
        return;
    }
    qCDebug(lcXEmbed) << "docked" << client << icon->id();
    m_icons.insert(client, icon);
}

void XEmbedProxy::undock(xcb_window_t client)
{
    if (XEmbedIcon *icon = m_icons.take(client)) {
        qCDebug(lcXEmbed) << "undocked" << client;
        delete icon;
    }
}

xcb_window_t XEmbedProxy::createContainer()
{
    const xcb_window_t window = xcb_generate_id(m_connection);
    // Transparent and outside every screen; it is only moved under the
    // pointer for the moment of a click.
    const uint32_t values[] = { 0, 0, 1, XCB_EVENT_MASK_NO_EVENT, m_colormap };
    uint32_t mask
        = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    if (m_colormap)
        mask |= XCB_CW_COLORMAP;
    xcb_create_window(m_connection, m_depth, window, m_screen->root, HiddenPosition, HiddenPosition,
        IconSize, IconSize, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, m_visual, mask, values);
    xcb_map_window(m_connection, window);
    return window;
}

void XEmbedProxy::click(xcb_window_t container, xcb_window_t client, uint8_t button, QPoint pos)
{
    if (container == XCB_WINDOW_NONE)
        return;
    // Programs open their menus where the pointer is, and some check that
    // it really is over their icon: so the pointer goes to the click, and
    // the icon under it, on top, for a moment.
    const int x = qBound(0, pos.x(), m_rootSize.width() - 1);
    const int y = qBound(0, pos.y(), m_rootSize.height() - 1);
    const int left = x - IconSize / 2;
    const int top = y - IconSize / 2;
    const uint32_t place[] = { uint32_t(left), uint32_t(top), XCB_STACK_MODE_ABOVE };
    xcb_configure_window(m_connection, container,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE, place);
    xcb_warp_pointer(m_connection, XCB_NONE, m_screen->root, 0, 0, 0, 0, x, y);

    // The click itself is sent to the icon. XTEST's fake input would not
    // reach it: Xwayland only passes input to X11 windows while the
    // compositor's pointer is over one, and here it is over the taskbar.
    // The pointer enters the icon first, as GTK only takes a click on a
    // window it has seen the pointer enter, and leaves it after. With no
    // event mask, each event goes to the program that made the icon,
    // whatever it selected: GTK 3, which selects only XInput 2 events,
    // still takes core events that others send it.
    const xcb_timestamp_t time = serverTime(); // so double clicks count
    const uint16_t buttonMask = uint16_t(XCB_BUTTON_MASK_1 << (button - 1));
    auto crossing = [&](uint8_t type) {
        xcb_enter_notify_event_t event = {};
        event.response_type = type;
        event.detail = XCB_NOTIFY_DETAIL_NONLINEAR;
        event.time = time;
        event.root = m_screen->root;
        event.event = client;
        event.root_x = int16_t(x);
        event.root_y = int16_t(y);
        event.event_x = int16_t(x - left);
        event.event_y = int16_t(y - top);
        event.mode = XCB_NOTIFY_MODE_NORMAL;
        event.same_screen_focus = 0x02; // same screen
        xcb_send_event(m_connection, false, client, XCB_EVENT_MASK_NO_EVENT,
            reinterpret_cast<const char *>(&event));
    };
    auto buttonEvent = [&](uint8_t type) {
        xcb_button_press_event_t event = {};
        event.response_type = type;
        event.detail = button;
        event.time = time;
        event.root = m_screen->root;
        event.event = client;
        event.root_x = int16_t(x);
        event.root_y = int16_t(y);
        event.event_x = int16_t(x - left);
        event.event_y = int16_t(y - top);
        event.state = type == XCB_BUTTON_RELEASE ? buttonMask : 0;
        event.same_screen = 1;
        xcb_send_event(m_connection, false, client, XCB_EVENT_MASK_NO_EVENT,
            reinterpret_cast<const char *>(&event));
    };
    // A menu opened now is watched, to close it on a click elsewhere (see
    // popupMapped).
    m_menuTimer.start();
    crossing(XCB_ENTER_NOTIFY);
    buttonEvent(XCB_BUTTON_PRESS);
    buttonEvent(XCB_BUTTON_RELEASE);
    crossing(XCB_LEAVE_NOTIFY);
    xcb_flush(m_connection);

    // The program handles the click in its own time; until then the icon
    // stays under the pointer (a later click keeps it there for its own).
    const int serial = ++m_clicks[container];
    QTimer::singleShot(ClickTime, this, [this, container, serial] {
        if (m_clicks.value(container) == serial) {
            m_clicks.remove(container);
            hideContainer(container);
        }
    });
}

void XEmbedProxy::popupMapped(xcb_window_t window)
{
    // Only the first menu (not its submenus) opened soon after a click on
    // an icon. Not only by the icon's own program: Wine's icons belong to
    // its explorer.exe, their menus to the program.
    if (m_menu != XCB_WINDOW_NONE || !m_menuTimer.isValid() || m_menuTimer.elapsed() > MenuTime)
        return;
    auto geometry = reply(
        xcb_get_geometry_reply(m_connection, xcb_get_geometry(m_connection, window), nullptr));
    if (!geometry || geometry->width < 8 || geometry->height < 8)
        return; // not a menu
    // Clicks on Wayland windows never reach X11 programs, so a menu would
    // stay open when the desktop or another window is clicked, as nothing
    // tells the program. A transparent window under the menu, over the
    // whole screen, takes those clicks into X11 instead: the menu's
    // pointer grab then sees a click beside the menu and closes it. It goes
    // when the menu does.
    m_menu = window;
    if (m_catcher == XCB_WINDOW_NONE) {
        m_catcher = xcb_generate_id(m_connection);
        const uint32_t values[] = { 0, 0, 1, XCB_EVENT_MASK_BUTTON_PRESS, m_colormap };
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT
            | XCB_CW_EVENT_MASK;
        if (m_colormap)
            mask |= XCB_CW_COLORMAP;
        xcb_create_window(m_connection, m_depth, m_catcher, m_screen->root, 0, 0, 1, 1, 0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, m_visual, mask, values);
    }
    const uint32_t place[] = { 0, 0, uint32_t(m_rootSize.width()), uint32_t(m_rootSize.height()),
        window, XCB_STACK_MODE_BELOW };
    xcb_configure_window(m_connection, m_catcher,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
            | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
        place);
    xcb_map_window(m_connection, m_catcher);
    xcb_flush(m_connection);
}

void XEmbedProxy::hideCatcher()
{
    m_menu = XCB_WINDOW_NONE;
    if (m_catcher != XCB_WINDOW_NONE) {
        xcb_unmap_window(m_connection, m_catcher);
        xcb_flush(m_connection);
    }
}

xcb_timestamp_t XEmbedProxy::serverTime() const
{
    return m_serverTimeBase + xcb_timestamp_t(m_serverTimer.elapsed());
}

void XEmbedProxy::hideContainer(xcb_window_t container)
{
    const uint32_t hidden[]
        = { uint32_t(HiddenPosition), uint32_t(HiddenPosition), XCB_STACK_MODE_BELOW };
    xcb_configure_window(m_connection, container,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_STACK_MODE, hidden);
    xcb_flush(m_connection);
}

} // namespace Daytona
