// Copy of KWidgetAction from KDE 3.1

#include "kwidgetaction.h"

#if KDE_VERSION < 308

#include <kapplication.h>
#include <ktoolbar.h>
#include <kdebug.h>

KWidgetAction::KWidgetAction( QWidget* widget,
    const QString& text, const KShortcut& cut,
    const QObject* receiver, const char* slot,
    KActionCollection* parent, const char* name )
  : KAction( text, cut, receiver, slot, parent, name )
  , m_widget( widget )
  , m_autoSized( false )
{
}

KWidgetAction::~KWidgetAction()
{
}

void KWidgetAction::setAutoSized( bool autoSized )
{
  if( m_autoSized == autoSized )
    return;

  m_autoSized = autoSized;

  if( !m_widget || !isPlugged() )
    return;

  KToolBar* toolBar = (KToolBar*)m_widget->parent();
  int i = findContainer( toolBar );
  if ( i == -1 )
    return;
  int id = itemId( i );

  toolBar->setItemAutoSized( id, m_autoSized );
}

int KWidgetAction::plug( QWidget* w, int index )
{
  if (kapp && !kapp->authorizeKAction(name()))
      return -1;

  if ( !w->inherits( "KToolBar" ) ) {
    kdError() << "KWidgetAction::plug: KWidgetAction must be plugged into KToolBar." << endl;
    return -1;
  }
  if ( !m_widget ) {
    kdError() << "KWidgetAction::plug: Widget was deleted or null!" << endl;
    return -1;
  }

  KToolBar* toolBar = static_cast<KToolBar*>( w );

  int id = KAction::getToolButtonID();

  m_widget->reparent( toolBar, QPoint() );
  toolBar->insertWidget( id, 0, m_widget, index );
  toolBar->setItemAutoSized( id, m_autoSized );

  addContainer( toolBar, id );

  // signal not available in KDE 3.0.x
  // connect( toolBar, SIGNAL( toolbarDestroyed() ), this, SLOT( slotToolbarDestroyed() ) );
  connect( toolBar, SIGNAL( destroyed() ), this, SLOT( slotDestroyed() ) );

  return containerCount() - 1;
}

void KWidgetAction::unplug( QWidget *w )
{
  if( !m_widget || !isPlugged() )
    return;

  KToolBar* toolBar = (KToolBar*)m_widget->parent();
  if ( toolBar == w )
  {
      //disconnect( toolBar, SIGNAL( toolbarDestroyed() ), this, SLOT( slotToolbarDestroyed() ) );
      m_widget->reparent( 0L, QPoint(), false /*showIt*/ );
  }
  KAction::unplug( w );
}

void KWidgetAction::slotToolbarDestroyed()
{
  Q_ASSERT( m_widget );
  Q_ASSERT( isPlugged() );
  if( !m_widget || !isPlugged() )
    return;

  // Don't let a toolbar being destroyed, delete my widget.
  m_widget->reparent( 0L, QPoint(), false /*showIt*/ );
}

void KWidgetAction::virtual_hook( int id, void* data )
{ KAction::virtual_hook( id, data ); }

// needed for conditional compilation
// otherwise, kwidgetaction.moc.o will even included in KDE 3.1 version
#include "kwidgetaction.moc"

#endif
