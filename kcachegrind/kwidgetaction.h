// Copy of KWidgetAction from KDE 3.1

#ifndef KWIDGETACTION_H
#define KWIDGETACTION_H

#include <kdeversion.h>
#include <kaction.h>

#if KDE_VERSION < 308

#include <qguardedptr.h>

/**
 * An action that automatically embeds a widget into a
 * toolbar.
 */
class KWidgetAction : public KAction
{
    Q_OBJECT
public:
    /**
     * Create an action that will embed widget into a toolbar
     * when plugged. This action may only be plugged into
     * a toolbar.
     */
    KWidgetAction( QWidget* widget, const QString& text,
                   const KShortcut& cut,
                   const QObject* receiver, const char* slot,
                   KActionCollection* parent, const char* name );
    virtual ~KWidgetAction();

    /**
     * Returns the widget associated with this action.
     */
    QWidget* widget() const { return m_widget; }

    void setAutoSized( bool );

    /**
     * Plug the action. The widget passed to the constructor
     * will be reparented to w, which must inherit KToolBar.
     */
    virtual int plug( QWidget* w, int index = -1 );
    /**
     * Unplug the action. Ensures that the action is not
     * destroyed. It will be hidden and reparented to 0L instead.
     */
    virtual void unplug( QWidget *w );
protected slots:
    void slotToolbarDestroyed();
private:
    QGuardedPtr<QWidget> m_widget;
    bool                 m_autoSized;
protected:
    virtual void virtual_hook( int id, void* data );
private:
    class KWidgetActionPrivate;
    KWidgetActionPrivate *d;
};

#endif // KDE_VERSION

#endif // KWIDGETACTION_H

