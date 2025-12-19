#include "mpvitem.h"

MpvItem::MpvItem(QQuickItem *parent)
    : MpvAbstractItem(parent)
{
    // Critical: Set vo=libmpv for Qt integration
    Q_EMIT setProperty("vo", "libmpv");

    // Verbose logging
    Q_EMIT setProperty("terminal", "yes");
    Q_EMIT setProperty("msg-level", "all=v");
}
