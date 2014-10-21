/*
* This file is part of Katepart
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MULTICURSOR_VIEW_H
#define MULTICURSOR_VIEW_H

#include <vector>
#include <memory>

#include <QObject>

#include <KXMLGUIClient>
#include <KTextEditor/Attribute>
#include <KTextEditor/MovingRange>

namespace KTextEditor
{
  class View;
  class Document;
  class MovingRange;
  class MovingInterface;
}

class MultiCursorView;


class MultiCursorView
: public QObject
, public KXMLGUIClient
{
  Q_OBJECT

  class CursorListDetail;
public:
  explicit MultiCursorView(
    KTextEditor::View *view
  , KTextEditor::Attribute::Ptr cursor_attr
  , KTextEditor::Attribute::Ptr selection_attr);
  ~MultiCursorView();

private:
  struct Cursor
  {
    Cursor(KTextEditor::MovingRange * range) noexcept
    : m_range(range)
    {}

    const KTextEditor::MovingCursor& cursor() const
    { return m_range->start(); }

    int line() const
    { return cursor().line(); }

    int column() const
    { return cursor().column(); }

    void setCursor(const KTextEditor::Cursor& cursor)
    { m_range->setRange(KTextEditor::Range(cursor, cursor.line(), cursor.column()+1)); }

    void setCursor(int line, int column)
    { m_range->setRange(KTextEditor::Range(line, column, line, column+1)); }

    bool operator==(const KTextEditor::Cursor& cursor) const
    { return this->cursor() == cursor; }

    bool operator<(const KTextEditor::Cursor& cursor) const
    { return this->cursor() < cursor; }

    bool operator==(const Cursor& other) const
    { return this->cursor() == other.cursor(); }

    bool operator<(const Cursor& other) const
    { return this->cursor() < other.cursor(); }

    friend bool operator<(const KTextEditor::Cursor& c1, const Cursor & c2)
    { return c1 < c2.cursor(); }

    void revalid()
    { setCursor(line(), column()); }

  private:
    std::unique_ptr<KTextEditor::MovingRange> m_range;
  };

  struct Range
  {
    Range(KTextEditor::MovingRange * range) noexcept
    : m_range(range)
    {}

    const KTextEditor::MovingCursor& start() const
    { return m_range->start(); }
    const KTextEditor::MovingCursor& end() const
    { return m_range->end(); }

    void setRange(const KTextEditor::Range & r)
    { m_range->setRange(r); }

    void setRange(const KTextEditor::Cursor & c1, const KTextEditor::Cursor & c2)
    { m_range->setRange(c1, c2); }

    bool contains(const KTextEditor::Range & range) const
    { return m_range->contains(range); }

    const KTextEditor::Range toRange() const
    { return m_range->toRange(); }

    bool isEmpty() const
    { return m_range->isEmpty(); }

  private:
    std::unique_ptr<KTextEditor::MovingRange> m_range;
  };
  ///TODO boost::flat_set ?
  typedef std::vector<Cursor> CursorList;
  typedef std::vector<Range> RangeList;

private slots:
  void exclusiveEditStart(KTextEditor::Document*);
  void exclusiveEditEnd(KTextEditor::Document*);

  void textInserted(KTextEditor::Document*, const KTextEditor::Range&);
  void textRemoved(KTextEditor::Document*, const KTextEditor::Range&, const QString&);
  void textBackspace();
  void textDelete();

  void setCursor();
  void removeCursorsOnLine();
  void removeAllCursors();

  void moveToNextCursor();
  void moveToPreviousCursor();

  void setActiveCursor();

  void setSynchronizedCursors();

  void moveCursorToUp();
  void moveCursorToDown();
  void moveCursorToRight();
  void moveCursorToLeft();

  void moveCursorToBeginningOfLine();
  void moveCursorToEndOfLine();
  void moveCursorToMatchingBracket();
  void moveCursorToWordRight();
  void moveCursorToWordLeft();

  void copyLinesWithCursor();
  void cutLinesWithCursor();
  void pasteLinesOnCursors();

  void extendLeftSelection();
  void extendRightSelection();
  void reduceLeftSelection();
  void reduceRightSelection();

  void setRange();
  void removeAllRanges();
  void removeRangesOnline();
  void clearRanges();

  void cutRanges();
  void copyRanges();
  void pasteRanges();

  void moveToNextStartRange();
  void moveToPreviousStartRange();
  void moveToNextEndRange();
  void moveToPreviousEndRange();

  void rangesFromCursors();

  void setSynchronizedRanges();

  void selectLineUp();
  void selectLineDown();
  void selectCharRight();
  void selectCharLeft();

  void selectBeginningOfLine();
  void selectEndOfLine();
  void selectWordRight();
  void selectWordLeft();
  //void selectPageUp();
  //void selectPageDown();
  //void selectMatchingBracket();

private:
  bool endEditing();
  bool startEditing(bool check_active = true);

  bool eventFilter(QObject *obj, QEvent *ev);

  void setCursor(const KTextEditor::Cursor& cursor);

  void connectCursors();
  void disconnectCursors();
  void connectSynchronizedCursors();
  void disconnectSynchronizedCursors();
  void startCursors();
  void stopCursors();
  void checkCursors();
  void setEnabledCursors(bool);

  void connectRanges();
  void disconnectRanges();
  void connectSynchronizedRanges();
  void disconnectSynchronizedRanges();
  void startRanges();
  void stopRanges();
  void checkRanges();
  void setEnabledRanges(bool);

  void setRange(const KTextEditor::Range& range, bool remove_if_contains = 1);
  void removeRange(RangeList::iterator, const KTextEditor::Range& range);

  KTextEditor::MovingRange * newMovingCursor(
    KTextEditor::Cursor const & cursor) const;
  KTextEditor::MovingRange * newMovingRange(
    KTextEditor::Range const & range) const;

public:
  void setActiveCursorCtrlClick(bool active, bool remove_cursor_if_only_click);
  void setActiveSelectionCtrlClick(bool active);

private:
  void setEventFilter(bool);

private:
  KTextEditor::View *m_view;
  KTextEditor::Document *m_document;
  KTextEditor::MovingInterface *m_smart;
  KTextEditor::Attribute::Ptr m_cursor_attr;
  KTextEditor::Attribute::Ptr m_selection_attr;
  CursorList m_cursors;
  RangeList m_ranges;
  bool m_has_exclusive_edit;
  bool m_is_active;
  bool m_is_synchronized_cursor;
  bool m_is_synchronized_selection;
  bool m_remove_cursor_if_only_click;
  bool m_has_cursor_ctrl;
  bool m_has_selection_ctrl;

  bool m_is_moved = false;
};

#endif
