/****************************************************************************
** Copied from Qt 5.6.3 sources, module qttools, directory src/shared/qtgradienteditor
** with small modifications to match more modern C++ standards and for no alpha-channel
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

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
