/*
    Copied from Qt 5.6.3 sources, module qttools, directory src/shared/qtgradienteditor
    with small modifications to match more modern C++ standards and for no alpha-channel

    This file is part of the tools applications of the Qt Toolkit.

    SPDX-FileCopyrightText: 2015 The Qt Company Ltd. <http://www.qt.io/licensing/>

    SPDX-License-Identifier: LGPL-2.1-only WITH Qt-LGPL-exception-1.1 OR LGPL-3.0-only WITH Qt-LGPL-exception-1.1 OR LicenseRef-Qt-Commercial
*/

#ifndef QTCOLORBUTTON_H
#define QTCOLORBUTTON_H

#include <QToolButton>

QT_BEGIN_NAMESPACE

class QtColorButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(bool backgroundCheckered READ isBackgroundCheckered WRITE setBackgroundCheckered)
public:
    explicit QtColorButton(QWidget *parent = nullptr);
    ~QtColorButton() override;

    bool isBackgroundCheckered() const;
    void setBackgroundCheckered(bool checkered);

    QColor color() const;

public Q_SLOTS:

    void setColor(const QColor &color);

Q_SIGNALS:
    void colorChanged(const QColor &color);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
#ifndef QT_NO_DRAGANDDROP
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#endif
private:
    QScopedPointer<class QtColorButtonPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtColorButton)
    Q_DISABLE_COPY(QtColorButton)
    Q_PRIVATE_SLOT(d_func(), void slotEditColor())
};

QT_END_NAMESPACE

#endif
