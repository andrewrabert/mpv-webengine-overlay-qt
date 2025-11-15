#ifndef MPVITEM_H
#define MPVITEM_H

#include <MpvAbstractItem>

class MpvItem : public MpvAbstractItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
};

#endif // MPVITEM_H
