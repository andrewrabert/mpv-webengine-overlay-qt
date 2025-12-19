#ifndef MPVITEM_H
#define MPVITEM_H

#include <MpvVulkanItem>

class MpvItem : public MpvVulkanItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
};

#endif // MPVITEM_H
