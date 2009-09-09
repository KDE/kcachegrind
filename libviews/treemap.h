/* This file is part of KCachegrind.
   Copyright (C) 2002-2009 Josef Weidendorfer <Josef.Weidendorfer@gmx.de>

   KCachegrind is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

/**
 * A Widget for visualizing hierarchical metrics as areas.
 * The API is similar to QListView.
 *
 * This file defines the following classes:
 *  DrawParams, RectDrawing, TreeMapItem, TreeMapWidget
 *
 * DrawParams/RectDrawing allows reusing of TreeMap drawing
 * functions in other widgets.
 */

#ifndef TREEMAP_H
#define TREEMAP_H

#include <QString>
#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QStringList>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>

class QMenu;
class Q3PopupMenu;
class TreeMapWidget;
class TreeMapItem;
class TreeMapItemList;


/**
 * Drawing parameters for an object.
 * A Helper Interface for RectDrawing.
 */
class DrawParams
{
public:
  /**
   * Positions for drawing into a rectangle.
   *
   * The specified position assumes no rotation.
   * If there is more than one text for one position, it is put
   * nearer to the center of the item.
   *
   * Drawing at top positions cuts free space from top,
   * drawing at bottom positions cuts from bottom.
   * Default usually gives positions clockwise according to field number.
   */
  enum Position { TopLeft, TopCenter, TopRight,
                  BottomLeft, BottomCenter, BottomRight,
                  Default, Unknown};

  // no constructor as this is an abstract class
  virtual ~DrawParams() {}

  virtual QString  text(int) const = 0;
  virtual QPixmap  pixmap(int) const = 0;
  virtual Position position(int) const = 0;
  // 0: no limit, negative: leave at least -maxLines() free
  virtual int      maxLines(int) const { return 0; }
  virtual int      fieldCount() const { return 0; }

  virtual QColor   backColor() const { return Qt::white; }
  virtual const QFont& font() const = 0;

  virtual bool selected() const { return false; }
  virtual bool current() const { return false; }
  virtual bool shaded() const { return true; }
  virtual bool rotated() const { return false; }
  virtual bool drawFrame() const { return true; }
};


/*
 * DrawParam with attributes stored
 */
class StoredDrawParams: public DrawParams
{
public:
  StoredDrawParams();
  explicit StoredDrawParams(const QColor& c,
			    bool selected = false, bool current = false);

  // getters
  QString  text(int) const;
  QPixmap  pixmap(int) const;
  Position position(int) const;
  int      maxLines(int) const;
  int      fieldCount() const { return _field.size(); }

  QColor   backColor() const { return _backColor; }
  bool selected() const { return _selected; }
  bool current() const { return _current; }
  bool shaded() const { return _shaded; }
  bool rotated() const { return _rotated; }
  bool drawFrame() const { return _drawFrame; }

  const QFont& font() const;

  // attribute setters
  void setField(int f, const QString& t, const QPixmap& pm = QPixmap(),
                Position p = Default, int maxLines = 0);
  void setText(int f, const QString&);
  void setPixmap(int f, const QPixmap&);
  void setPosition(int f, Position);
  void setMaxLines(int f, int);
  void setBackColor(const QColor& c) { _backColor = c; }
  void setSelected(bool b) { _selected = b; }
  void setCurrent(bool b) { _current = b; }
  void setShaded(bool b) { _shaded = b; }
  void setRotated(bool b) { _rotated = b; }
  void drawFrame(bool b) { _drawFrame = b; }

protected:
  QColor _backColor;
  bool _selected :1;
  bool _current :1;
  bool _shaded :1;
  bool _rotated :1;
  bool _drawFrame :1;

private:
  // resize field array if needed to allow to access field <f>
  void ensureField(int f);

  struct Field {
    QString text;
    QPixmap pix;
    Position pos;
    int maxLines;
  };

  QVector<Field> _field;
};


/* State for drawing on a rectangle.
 *
 * Following drawing functions are provided:
 * - background drawing with shading and 3D frame
 * - successive pixmap/text drawing at various positions with wrap-around
 *   optimized for minimal space usage (e.g. if a text is drawn at top right
 *   after text on top left, the same line is used if space allows)
 *
 */
class RectDrawing
{
public:
  RectDrawing(const QRect&);
  ~RectDrawing();

  // The default DrawParams object used.
  DrawParams* drawParams();
  // we take control over the given object (i.e. delete at destruction)
  void setDrawParams(DrawParams*);

  // draw on a given QPainter, use this class as info provider per default
  void drawBack(QPainter*, DrawParams* dp = 0);
  /* Draw field at position() from pixmap()/text() with maxLines().
   * Returns true if something was drawn
   */
  bool drawField(QPainter*, int f, DrawParams* dp = 0);

  // resets rectangle for free space
  void setRect(const QRect&);

  // Returns the rectangle area still free of text/pixmaps after
  // a number of drawText() calls.
  QRect remainingRect(DrawParams* dp = 0);

private:
  int _usedTopLeft, _usedTopCenter, _usedTopRight;
  int _usedBottomLeft, _usedBottomCenter, _usedBottomRight;
  QRect _rect;

  // temporary
  int _fontHeight;
  QFontMetrics* _fm;
  DrawParams* _dp;
};


class TreeMapItemList: public QList<TreeMapItem*>
{
public:
  TreeMapItem* commonParent();
};


/**
 * Base class of items in TreeMap.
 *
 * This class supports an arbitrary number of text() strings
 * positioned counterclock-wise starting at TopLeft. Each item
 * has its own static value(), sum() and sorting(). The
 * splitMode() and borderWidth() is taken from a TreeMapWidget.
 *
 * If you want more flexibility, reimplement TreeMapItem and
 * override the corresponding methods. For dynamic creation of child
 * items on demand, reimplement children().
 */
class TreeMapItem: public StoredDrawParams
{
public:

  /**
   * Split direction for nested areas:
   *  AlwaysBest: Choose split direction for every subitem according to
   *              longest side of rectangle left for drawing
   *  Best:       Choose split direction for all subitems of an area
   *              depending on longest side
   *  HAlternate: Horizontal at top;  alternate direction on depth step
   *  VAlternate: Vertical at top; alternate direction on depth step
   *  Horizontal: Always horizontal split direction
   *  Vertical:   Always vertical split direction
   */
  enum SplitMode { Bisection, Columns, Rows,
                   AlwaysBest, Best,
                   HAlternate, VAlternate,
                   Horizontal, Vertical };

  explicit TreeMapItem(TreeMapItem* parent = 0, double value = 1.0 );
  TreeMapItem(TreeMapItem* parent, double value,
              const QString& text1, const QString& text2 = QString(),
              const QString& text3 = QString(), const QString& text4 = QString());
  virtual ~TreeMapItem();

  bool isChildOf(TreeMapItem*);

  TreeMapItem* commonParent(TreeMapItem* item);

  // force a redraw of this item
  void redraw();

  // delete all children
  void clear();

  // force new child generation & refresh
  void refresh();

  // call in a reimplemented items() method to check if already called
  // after a clear(), this will return false
  bool initialized();

  /**
   * Adds an item to a parent.
   * When no sorting is used, the item is appended (drawn at bottom).
   * This is only needed if the parent was not already specified in the
   * construction of the item.
   */
  void addItem(TreeMapItem*);

  /**
   * Returns a list of text strings of specified text number,
   * from root up to this item.
   */
  QStringList path(int) const;

  /**
   * Depth of this item. This is the distance to root.
   */
  int depth() const;

  /**
   * Parent Item
   */
  TreeMapItem* parent() const { return _parent; }

  /**
   * Temporary rectangle used for drawing this item the last time.
   * This is internally used to map from a point to an item.
   */
  void setItemRect(const QRect& r) { _rect = r; }
  void clearItemRect();
  const QRect& itemRect() const { return _rect; }
  int width() const { return _rect.width(); }
  int height() const { return _rect.height(); }

  /**
   * Temporary rectangle list of free space of this item.
   * Used internally to enable tooltip.
   */
  void clearFreeRects();
  const QList<QRect>& freeRects() const { return _freeRects; }
  void addFreeRect(const QRect& r);

  /**
   * Temporary child item index of the child that was current() recently.
   */
  int index() const { return _index; }
  void setIndex(int i) { _index = i; }


  /**
   * TreeMap widget this item is put in.
   */
  TreeMapWidget* widget() const { return _widget; }

  void setParent(TreeMapItem* p);
  void setWidget(TreeMapWidget* w) { _widget = w; }
  void setSum(double s) { _sum = s; }
  void setValue(double s) { _value = s; }

  virtual double sum() const;
  virtual double value() const;
  // replace "Default" position with setting from TreeMapWidget
  virtual Position position(int) const;
  virtual const QFont& font() const;
  virtual bool isMarked(int) const;

  virtual int borderWidth() const;

  /**
   * Returns the text number after that sorting is done or
   * -1 for no sorting, -2 for value() sorting (default).
   * If ascending != 0, a bool value is written at that location
   * to indicate if sorting should be ascending.
   */
  virtual int sorting(bool* ascending) const;

  /**
   * Set the sorting for child drawing.
   *
   * Default is no sorting: <textNo> = -1
   * For value() sorting, use <textNo> = -2
   *
   * For fast sorting, set this to -1 before child insertions and call
   * again after inserting all children.
   */
  void setSorting(int textNo, bool ascending = true);

  /**
   * Resort according to the already set sorting.
   *
   * This has to be done if the sorting base changes (e.g. text or values
   * change). If this is only true for the children of this item, you can
   * set the recursive parameter to false.
   */
  void resort(bool recursive = true);

  virtual SplitMode splitMode() const;
  virtual int rtti() const;
  // not const as this can create children on demand
  virtual TreeMapItemList* children();

protected:
  TreeMapItemList* _children;
  double _sum, _value;

private:
  TreeMapWidget* _widget;
  TreeMapItem* _parent;

  int _sortTextNo;
  bool _sortAscending;

  // temporary layout
  QRect _rect;
  QList<QRect> _freeRects;
  int _depth;

  // temporary self value (when using level skipping)
  double _unused_self;

  // index of last active subitem
  int _index;
};


/**
 * Class for visualization of a metric of hierarchically
 * nested items as 2D areas.
 */
class TreeMapWidget: public QWidget
{
  Q_OBJECT

public:

  /**
   * Same as in QListBox/QListView
   */
  enum SelectionMode { Single, Multi, Extended, NoSelection };

  /* The widget gets owner of the base item */
  explicit TreeMapWidget(TreeMapItem* base, QWidget* parent=0);
  ~TreeMapWidget();

  /**
   * Returns the TreeMapItem filling out the widget space
   */
  TreeMapItem* base() const { return _base; }

  /**
   * Returns a reference to the current widget font.
   */
  const QFont& currentFont() const;

  /**
   * Returns the area item at position x/y, independent from any
   * maxSelectDepth setting.
   */
  TreeMapItem* item(int x, int y) const;

  /**
   * Returns the nearest item with a visible area; this
   * can be the given item itself.
   */
  TreeMapItem* visibleItem(TreeMapItem*) const;

  /**
   * Returns the item possible for selection. this returns the
   * given item itself or a parent thereof,
   * depending on setting of maxSelectDepth().
   */
  TreeMapItem* possibleSelection(TreeMapItem*) const;

  /**
   * Selects or unselects an item.
   * In multiselection mode, the constrain that a selected item
   * has no selected children or parents stays true.
   */
  void setSelected(TreeMapItem*, bool selected = true);

  /**
   * Switches on the marking <markNo>. Marking 0 switches off marking.
   * This is mutually exclusive to selection, and is automatically
   * switched off when selection is changed (also by the user).
   * Marking is visually the same as selection, and is based on
   * TreeMapItem::isMarked(<markNo>).
   * This enables to programmatically show multiple selected items
   * at once even in single selection mode.
   */
  void setMarked(int markNo = 1, bool redraw = true);

  /**
   * Clear selection of all selected items which are children of
   * parent. When parent == 0, clears whole selection
   * Returns true if selection changed.
   */
  bool clearSelection(TreeMapItem* parent = 0);

  /**
   * Selects or unselects items in a range.
   * This is needed internally for Shift-Click in Extended mode.
   * Range means for a hierarchical widget:
   * - select/unselect i1 and i2 according selected
   * - search common parent of i1 and i2, and select/unselect the
   *   range of direct children between but excluding the child
   *   leading to i1 and the child leading to i2.
   */
  void setRangeSelection(TreeMapItem* i1,
                         TreeMapItem* i2, bool selected);

  /**
   * Sets the current item.
   * The current item is mainly used for keyboard navigation.
   */
  void setCurrent(TreeMapItem*, bool kbd=false);

  /**
   * Set the maximal depth a selected item can have.
   * If you try to select a item with higher depth, the ancestor holding
   * this condition is used.
   *
   * See also possibleSelection().
   */
  void setMaxSelectDepth(int d) { _maxSelectDepth = d; }


  void setSelectionMode(SelectionMode m) { _selectionMode = m; }

  /**
   * for setting/getting global split direction
   */
  void setSplitMode(TreeMapItem::SplitMode m);
  TreeMapItem::SplitMode splitMode() const;
  // returns true if string was recognized
  bool setSplitMode(const QString&);
  QString splitModeString() const;


  /*
   * Shading of rectangles enabled ?
   */
  void setShadingEnabled(bool s);
  bool isShadingEnabled() const { return _shading; }

  /* Setting for a whole depth level: draw 3D frame (default) or solid */
  void drawFrame(int d, bool b);
  bool drawFrame(int d) const { return (d<4)?_drawFrame[d]:true; }

  /* Setting for a whole depth level: draw items (default) or transparent */
  void setTransparent(int d, bool b);
  bool isTransparent(int d) const { return (d<4)?_transparent[d]:false; }

  /**
   * Items usually have a size proportional to their value().
   * With <width>, you can give the minimum width
   * of the resulting rectangle to still be drawn.
   * For space not used because of to small items, you can specify
   * with <reuseSpace> if the background should shine through or
   * the space will be used to enlarge the next item to be drawn
   * at this level.
   */
  void setVisibleWidth(int width, bool reuseSpace = false);

  /**
   * If a children value() is almost the parents sum(),
   * it can happen that the border to be drawn for visibilty of
   * nesting relations takes to much space, and the
   * parent/child size relation can not be mapped to a correct
   * area size relation.
   *
   * Either
   * (1) Ignore the incorrect drawing, or
   * (2) Skip drawing of the parent level alltogether.
   */
  void setSkipIncorrectBorder(bool enable = true);
  bool skipIncorrectBorder() const { return _skipIncorrectBorder; }

  /**
   * Maximal nesting depth
   */
  void setMaxDrawingDepth(int d);
  int maxDrawingDepth() const { return _maxDrawingDepth; }

  /**
   * Minimal area for rectangles to draw
   */
  void setMinimalArea(int area);
  int minimalArea() const { return _minimalArea; }

  /* defaults for text attributes */
  QString defaultFieldType(int) const;
  QString defaultFieldStop(int) const;
  bool    defaultFieldVisible(int) const;
  bool    defaultFieldForced(int) const;
  DrawParams::Position defaultFieldPosition(int) const;

  /**
   * Set the type name of a field.
   * This is important for the visualization menu generated
   * with visualizationMenu()
   */
  void setFieldType(int, const QString&);
  QString fieldType(int) const;

  /**
   * Stop drawing at item with name
   */
  void setFieldStop(int, const QString&);
  QString fieldStop(int) const;

  /**
   * Should the text with number textNo be visible?
   * This is only done if remaining space is enough to allow for
   * proportional size constrains.
   */
  void setFieldVisible(int, bool);
  bool fieldVisible(int) const;

  /**
   * Should the drawing of the name into the rectangle be forced?
   * This enables drawing of the name before drawing subitems, and
   * thus destroys proportional constrains.
   */
  void setFieldForced(int, bool);
  bool fieldForced(int) const;

  /**
   * Set the field position in the area. See TreeMapItem::Position
   */
  void setFieldPosition(int, DrawParams::Position);
  DrawParams::Position fieldPosition(int) const;
  void setFieldPosition(int, const QString&);
  QString fieldPositionString(int) const;

  /**
   * Do we allow the texts to be rotated by 90 degrees for better fitting?
   */
  void setAllowRotation(bool);
  bool allowRotation() const { return _allowRotation; }

  void setBorderWidth(int w);
  int borderWidth() const { return _borderWidth; }

  /**
   * Populate given menu with option items.
   * The added items are automatically connected to handlers.
   */
  void addSplitDirectionItems(QMenu*);

  TreeMapWidget* widget() { return this; }
  TreeMapItem* current() const { return _current; }
  TreeMapItemList selection() const { return _selection; }
  bool isSelected(TreeMapItem* i) const;
  int maxSelectDepth() const { return _maxSelectDepth; }
  SelectionMode selectionMode() const { return _selectionMode; }

  /**
   * Return tooltip string to show for a item (can be rich text)
   * Default implementation gives lines with "text0 (text1)" going to root.
   */
  virtual QString tipString(TreeMapItem* i) const;

  /**
   * Redraws an item with all children.
   * This takes changed values(), sums(), colors() and text() into account.
   */
  void redraw(TreeMapItem*);
  void redraw() { redraw(_base); }

  /**
   * Resort all TreeMapItems. See TreeMapItem::resort().
   */
  void resort() { _base->resort(true); }

  // internal
  void drawTreeMap();

  // used internally when items are destroyed
  void deletingItem(TreeMapItem*);

protected slots:
  void splitActivated(QAction*);

signals:
  void selectionChanged();
  void selectionChanged(TreeMapItem*);

  /**
   * This signal is emitted if the current item changes.
   * If the change is done because of keyboard navigation,
   * the <kbd> is set to true
   */
  void currentChanged(TreeMapItem*, bool keyboard);
  void clicked(TreeMapItem*);
  void returnPressed(TreeMapItem*);
  void doubleClicked(TreeMapItem*);
  void rightButtonPressed(TreeMapItem*, const QPoint &);
  void contextMenuRequested(TreeMapItem*, const QPoint &);

protected:
  void mousePressEvent( QMouseEvent * );
  void contextMenuEvent( QContextMenuEvent * );
  void mouseReleaseEvent( QMouseEvent * );
  void mouseMoveEvent( QMouseEvent * );
  void mouseDoubleClickEvent( QMouseEvent * );
  void keyPressEvent( QKeyEvent* );
  void paintEvent( QPaintEvent * );
  void fontChange( const QFont& );
  bool event(QEvent *event);

private:
  TreeMapItemList diff(TreeMapItemList&, TreeMapItemList&);
  // returns true if selection changed
  TreeMapItem* setTmpSelected(TreeMapItem*, bool selected = true);
  TreeMapItem* setTmpRangeSelection(TreeMapItem* i1,
				    TreeMapItem* i2, bool selected);
  bool isTmpSelected(TreeMapItem* i);

  void drawItem(QPainter* p, TreeMapItem*);
  void drawItems(QPainter* p, TreeMapItem*);
  bool horizontal(TreeMapItem* i, const QRect& r);
  void drawFill(TreeMapItem*,QPainter* p, const QRect& r);
  void drawFill(TreeMapItem*,QPainter* p, const QRect& r,
		TreeMapItemList* list, int idx, int len, bool goBack);
  bool drawItemArray(QPainter* p, TreeMapItem*, const QRect& r, double,
		     TreeMapItemList* list, int idx, int len, bool);
  bool resizeAttr(int);

  void addSplitAction(QMenu*, const QString&, int);

  TreeMapItem* _base;
  TreeMapItem *_current, *_pressed, *_lastOver, *_oldCurrent;
  int _maxSelectDepth, _maxDrawingDepth;

  // attributes for field, per textNo
  struct FieldAttr {
    QString type, stop;
    bool visible, forced;
    DrawParams::Position pos;
  };
  QVector<FieldAttr> _attr;

  SelectionMode _selectionMode;
  TreeMapItem::SplitMode _splitMode;
  int _visibleWidth, _stopArea, _minimalArea, _borderWidth;
  bool _reuseSpace, _skipIncorrectBorder, _drawSeparators, _shading;
  bool _allowRotation;
  bool _transparent[4], _drawFrame[4];
  TreeMapItem * _needsRefresh;
  TreeMapItemList _selection;
  int _markNo;

  // temporary selection while dragging, used for drawing
  // most of the time, _selection == _tmpSelection
  TreeMapItemList _tmpSelection;
  bool _inShiftDrag, _inControlDrag;

  // temporary widget font metrics while drawing
  QFont _font;
  int _fontHeight;

  // back buffer pixmap
  QPixmap _pixmap;
};

#endif
