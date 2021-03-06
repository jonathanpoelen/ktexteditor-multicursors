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

#include "multicursorview.h"
#include "multicursorplugin.h"

#include <functional>
#include <algorithm>

#include <KTextEditor/View>
#include <KTextEditor/Document>
#include <KTextEditor/MovingInterface>
#include <KTextEditor/HighlightInterface>

#include <KAction>
#include <KActionCollection>

#include <QtGui/QApplication>
#include <QClipboard>
#include <QKeyEvent>

namespace {
template<class Cont, class T>
static typename Cont::iterator
lowerBound(Cont & cont, const T & x)
{ return std::lower_bound(cont.begin(), cont.end(), x); }

template<class Cont, class T, class Compare>
static typename Cont::iterator
lowerBound(Cont & cont, const T & x, Compare comp)
{ return std::lower_bound(cont.begin(), cont.end(), x, comp); }

template<class Cont>
void uniqueCont(Cont & cont)
{ cont.erase(std::unique(cont.begin(), cont.end()), cont.end()); }
}

struct MultiCursorView::CursorListDetail
{
  template<class Cont, class Checker>
  static void eraseInvalided(
    MultiCursorView & cursorview
  , Cont & cont
  , KTextEditor::MovingRange* range
  , Checker checker)
  {
    if (!cursorview.m_is_moved && !cursorview.m_has_exclusive_edit) {
      auto pos = std::find_if(
        cont.begin()
      , cont.end()
      , [range](typename Cont::value_type const & x){
        return x.isSame(range);
      });
      /*if (pos != cont.end())*/ {
        cont.erase(pos);
        checker();
      }
    }
  }

  template<class GetCursor1, class GetCursor2, class F>
  static void selectAlgo(
    bool b, MultiCursorView & mview, GetCursor1 get1, GetCursor2 get2, F f)
  {
    mview.m_ranges_temp.swap(mview.m_ranges);
    mview.m_ranges.clear();
    mview.m_ranges.reserve(mview.m_ranges_temp.size());

    if (b) {
      for (Range & r : mview.m_ranges_temp) {
        auto const & c = get1(r);
        KTextEditor::Range range(f(c.line(), c.column()), get2(r));
        mview.setRange(range, false);
      }
    }
    else {
      for (Range & r : mview.m_ranges_temp) {
        auto const & c = get2(r);
        KTextEditor::Range range(f(c.line(), c.column()), get1(r));
        mview.setRange(range, false);
      }
    }

    mview.m_ranges_temp.clear();
  }

  template<class F>
  static void selectAlgoLeft(MultiCursorView & mview, F f)
  {
    selectAlgo(
      mview.m_view->selectionRange().start() == mview.m_view->cursorPosition()
    , mview
    , CursorListDetail::RangeStart()
    , CursorListDetail::RangeEnd()
    , f);
  }

  template<class F>
  static void selectAlgoRight(MultiCursorView & mview, F f)
  {
    selectAlgo(
      mview.m_view->selectionRange().end() == mview.m_view->cursorPosition()
    , mview
    , CursorListDetail::RangeEnd()
    , CursorListDetail::RangeStart()
    , f);
  }

private:

  static bool isBracket(QChar c) {
    return c == '{'
        || c == '}'
        || c == '('
        || c == ')'
        || c == '['
        || c == ']'
    ;
  }

  template<class Mover>
  static bool matchingBracketImpl(
    KTextEditor::Document * doc
  , KTextEditor::HighlightInterface * highlight
  , KTextEditor::Cursor & cursor
  , QChar bracket
  , QChar opposite
  , Mover move)
  {
    const QString mode = highlight->highlightingModeAt(cursor);

    uint nesting = 0;
    while (move(cursor)) {
      if (highlight->highlightingModeAt(cursor) == mode) {
        const QChar c = doc->character(cursor);
        if (c == opposite) {
          if (nesting == 0) {
            return true;
          }
          nesting--;
        } else if (c == bracket) {
          nesting++;
        }
      }
    }
    return false;
  }

public:
  struct FakeHighlight : KTextEditor::HighlightInterface{
    KTextEditor::Attribute::Ptr defaultStyle(DefaultStyle) const
    { return {}; }
    QStringList embeddedHighlightingModes() const
    { return {}; }
    QString highlightingModeAt(const KTextEditor::Cursor&)
    { return ""; }
    QList< AttributeBlock > lineAttributes(const unsigned int)
    { return {}; }
  };

  static FakeHighlight & fake_highlight()
  {
    static FakeHighlight h;
    return h;
  }

  static bool matchingBracket(
    KTextEditor::Document * doc
  , KTextEditor::HighlightInterface * highlight
  , KTextEditor::Cursor & cursor
  , int maxLines = 5000)
  {
    int columns = doc->lineLength(cursor.line());
    if (!columns) {
      return false;
    }

    QChar bracket;

    const QChar right = doc->character(cursor);
    const QChar left  = doc->character(
      KTextEditor::Cursor(cursor.line(), cursor.column() - 1));

    if (isBracket(left)) {
      bracket = left;
      cursor.setColumn(cursor.column() - 1);
    } else if (isBracket(right)) {
      bracket = right;
    } else {
      return false;
    }

    if (bracket == '{' || bracket == '(' || bracket == '[') {
      const QChar opposite
        = (bracket == '{' ? '}' : (bracket == '(' ? ')' : ']'));
      const int maxLine = qMin(cursor.line() + maxLines, doc->lines());
      const bool res = matchingBracketImpl(
        doc, highlight, cursor, bracket, opposite
      , [doc, maxLine, &columns](KTextEditor::Cursor & cursor) {
        const int column = cursor.column() + 1;
        if (column == columns) {
          columns = doc->lineLength(cursor.line());
          const int line = cursor.line() + 1;
          if (line > maxLine) {
            return false;
          }
          cursor.setPosition(line, 0);
        }
        else {
          cursor.setColumn(column);
        }
        return true;
      });
      if (res) {
        cursor.setColumn(cursor.column() + 1);
      }
      return res;
    }
    else {
      const QChar opposite
        = (bracket == '}' ? '{' : (bracket == ')' ? '(' : '['));
      const int minLine = qMax(cursor.line() - maxLines, 0);
      return matchingBracketImpl(
        doc, highlight, cursor, bracket, opposite
      , [doc, minLine](KTextEditor::Cursor & cursor) {
        const int column = cursor.column() - 1;
        if (column == -1) {
          const int line = cursor.line() - 1;
          if (line < minLine) {
            return false;
          }
          cursor.setPosition(line, doc->lineLength(line));
        }
        else {
          cursor.setColumn(column);
        }
        return true;
      });
    }
  }

  static KTextEditor::Cursor wordPrev(
    KTextEditor::Document * doc, int line, int column)
  {
    const QString text_line = doc->line(line);

    if (column != 0) {
      while (column && text_line[column-1].isSpace()) {
        --column;
      }
    }

    if (column == 0) {
      if (line) {
        --line;
        column = doc->lineLength(line);
      }
    }
    else if (text_line[column-1].isLetterOrNumber()) {
      do {
        --column;
      } while (column && text_line[column-1].isLetterOrNumber());
    }
    else {
      do {
        --column;
      } while (column
        && !text_line[column-1].isLetterOrNumber()
        && !text_line[column-1].isSpace()
      );
    }

    return {line, column};
  }

  static KTextEditor::Cursor wordNext(
    KTextEditor::Document * doc, int line, int column)
  {
    const QString text_line = doc->line(line);
    const int max_line = doc->lineLength(line);

    if (column == max_line) {
      if (line + 1 != doc->lines()) {
        ++line;
        column = 0;
      }
    }
    else if (text_line[column].isLetterOrNumber()) {
      do {
        ++column;
      } while (column != max_line && text_line[column].isLetterOrNumber());
    }
    else {
      do {
        ++column;
      } while (column != max_line
        && !text_line[column].isLetterOrNumber()
        && !text_line[column].isSpace()
      );
    }

    while (column != max_line && text_line[column].isSpace()) {
      ++column;
    }

    return {line, column};
  }

  template<class It, class Gen>
  static It moveCursorImpl(It first, It last, It res, Gen createCursor)
  {
    res->setCursor(createCursor(*first));
    while (++first != last) {
      first->setCursor(createCursor(*first));
      if (!(*res == *first)) {
        *++res = std::move(*first);
      }
    }
    ++res;
    return res;
  }

  template<class Gen>
  static void moveCursor(
    MultiCursorView & view, CursorList::iterator first, Gen createCursor)
  {
    const auto last = view.m_cursors.end();
    if (first != last) {
      view.m_cursors.erase(
        moveCursorImpl(first, last, view.m_cursors.begin(), createCursor),
        last
      );
      view.checkCursors();
    }
    else {
      view.m_cursors.clear();
      view.stopCursors();
    }
  }

  struct RangeStart {
    const KTextEditor::MovingCursor&
    operator()(MultiCursorView::Range const & r) const
    { return r.start(); }
  };

  struct RangeEnd {
    const KTextEditor::MovingCursor&
    operator()(MultiCursorView::Range const & r) const
    { return r.end(); }
  };

  struct ProxyCursor {
    const KTextEditor::MovingCursor&
    operator()(MultiCursorView::Cursor const & c) const
    { return c.cursor(); }
  };

  template<class Container, class GetCursor>
  static void moveToNext(
    Container & cont, KTextEditor::View* view, GetCursor cur
  ) {
    typedef typename Container::value_type Value;
    auto it = lowerBound(cont, view->cursorPosition()
    , [cur](Value const & r, KTextEditor::Cursor const & c) {
      return cur(r) <= c;
    });
    view->setCursorPosition(
      it != cont.end() ? cur(*it) : cur(cont.front()));
  }

  template<class Container, class GetCursor>
  static void moveToPrevious(
    Container & cont, KTextEditor::View* view, GetCursor cur
  ) {
    typedef typename Container::value_type Value;
    auto it = lowerBound(cont, view->cursorPosition()
    , [cur](Value const & r, KTextEditor::Cursor const & c) {
      return cur(r) < c;
    });
    view->setCursorPosition(
      it != cont.begin() ? cur(*--it) : cur(cont.back()));
  }

  static RangeList::iterator lowerBoundEnd(
    RangeList & ranges, const KTextEditor::Cursor & cursor
  ) {
    return lowerBound(ranges, cursor
    , [](Range const & r, KTextEditor::Cursor const & c) {
        return r.end() < c;
      }
    );
  }
};


void MultiCursorView::InvalidedCursor
::rangeEmpty(KTextEditor::MovingRange* range)
{
  CursorListDetail::eraseInvalided(
    m_cursorview
  , m_cursorview.m_cursors
  , range
  , [this]() { m_cursorview.checkCursors(); });
}

void MultiCursorView::InvalidedRange
::rangeEmpty(KTextEditor::MovingRange* range)
{
  CursorListDetail::eraseInvalided(
    m_cursorview
  , m_cursorview.m_ranges
  , range
  , [this]() { m_cursorview.checkRanges(); });
}


MultiCursorView::MultiCursorView(
  KTextEditor::View *view
, KTextEditor::Attribute::Ptr cursor_attr
, KTextEditor::Attribute::Ptr selection_attr
)
: QObject(view)
, KXMLGUIClient(view)
, m_view(view)
, m_document(view->document())
, m_smart(qobject_cast<KTextEditor::MovingInterface*>(m_document))
, m_cursor_attr(cursor_attr)
, m_selection_attr(selection_attr)
, m_has_exclusive_edit(false)
, m_is_active(true)
, m_is_synchronized_cursor(false)
, m_is_synchronized_selection(false)
, m_remove_cursor_if_only_click(false)
, m_has_cursor_ctrl(false)
, m_has_selection_ctrl(false)
, m_remove_all_if_esc(false)
, m_is_moved(false)
, m_invalided_cursor(*this)
, m_invalided_range(*this)
{
	setComponentData(MultiCursorPluginFactory::componentData());

	KActionCollection* collection = actionCollection();
	KAction *action;

#define ENTRY(Text, Name, Receiver) \
	action = new KAction(i18n(Text), this);\
	collection->addAction(Name, action);\
	connect(action, SIGNAL(triggered()), this, SLOT(Receiver));

	/*ENTRY("info cursors", "info_multicursor", debug());
	action->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_I);*/

	ENTRY("Set Virtual Cursor", "set_multicursor", setCursor());
	action->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_C);

  ENTRY("Backspace Character on Virtuals Cursors", "backspace_multicursor", backspace());
	action->setShortcut(Qt::ALT + Qt::Key_Backspace);

  ENTRY("Delete Character on Virtuals Cursors", "delete_multicursor", deleteNextCharacter());
	action->setShortcut(Qt::ALT + Qt::Key_Delete);

  ENTRY("Remove All Virtuals Cursors", "remove_all_multicursor", removeAllCursors());
	action->setShortcut(Qt::ALT + Qt::SHIFT + Qt::Key_Delete);

  ENTRY("Remove Virtuals Cursors on Line", "remove_cursor_line_multicursor", removeCursorsOnLine());
	action->setShortcut(Qt::CTRL + Qt::ALT +  Qt::Key_Delete);

	ENTRY("Move to Next Virtual Cursor", "next_multicursor", moveToNextCursor());
	action->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_H);

  ENTRY("Move to Previous Virtual Cursor", "previous_multicursor", moveToPreviousCursor());
	action->setShortcut(Qt::CTRL + Qt::ALT + Qt::SHIFT + Qt::Key_H);

  ENTRY("Enable Virtuals Cursors", "active_multicursor", setActiveCursor())
	action->setCheckable(true);
	action->setChecked(true);

  ENTRY("Synchronize With the Blinking Cursor", "synchronise_multicursor", setSynchronizedCursors());
  action->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_P);
  action->setCheckable(true);

  ENTRY("Copy the Lines With a Virtual Cursor", "copy_line_with_cursor", copyLinesWithCursor());

  ENTRY("Cut the Lines With a Virtual Cursor", "cut_line_with_cursor", cutLinesWithCursor());

  ENTRY("Paste the Lines on a Virtual Cursor", "paste_line_with_cursor", pasteLinesOnCursors());

  ENTRY("Extend the Selection to Left", "extend_left_selection", extendLeftSelection());
  action->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_ParenLeft);

  ENTRY("Extend the Selection to Right", "extend_right_selection", extendRightSelection());
  action->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_ParenRight);

  ENTRY("Reduce the Selection of Left", "reduce_left_selection", reduceLeftSelection());

  ENTRY("Reduce the Selection of Right", "reduce_right_selection", reduceRightSelection());


  ENTRY("Set Virtual Selection", "set_multiselection", setRange());
  action->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_R);

  ENTRY("Deselect All Virtuals Selections", "remove_all_multiselection", removeAllRanges());
  action->setShortcut(Qt::CTRL + Qt::Key_Underscore);

  ENTRY("Deselect Virtuals Selections Line", "remove_line_multiselection", removeRangesOnline());

  ENTRY("Remove Text In a Virtuals Selections", "clear_multiselection", clearRanges());
  action->setShortcut(Qt::ALT + Qt::Key_Escape);

  ENTRY("Cut Virtuals Selections", "cut_multiselection", cutRanges());
  action->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_X);

  ENTRY("Copy Virtuals Selections", "copy_multiselection", copyRanges());
  action->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_C);

  ENTRY("Paste Virtuals Selections", "paste_multiselection", pasteRanges());
  action->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_V);

  ENTRY("Move to Next Virtual Selection Start", "next_start_multiselection", moveToNextStartRange());

  ENTRY("Move to Previous Virtual Selection Start", "previous_start_multiselection", moveToPreviousStartRange());

  ENTRY("Move to Next Virtual Selection End", "next_end_multiselection", moveToNextEndRange());

  ENTRY("Move to Previous Virtual Selection End", "previous_end_multiselection", moveToPreviousEndRange());

  ENTRY("Set Virtual Selection From Virtual Cursors", "from_cursor_multiselection", rangesFromCursors());
  action->setEnabled(false);

  ENTRY("Synchronize With the Selection", "synchronise_multiselection", setSynchronizedRanges());
  action->setCheckable(true);

#undef ENTRY

  setEnabledCursors(false);
	setEnabledRanges(false);
	setXMLFile("multicursorui.rc");
}

MultiCursorView::~MultiCursorView()
{}

void MultiCursorView::exclusiveEditStart(KTextEditor::Document *)
{
	m_has_exclusive_edit = true;
}

void MultiCursorView::exclusiveEditEnd(KTextEditor::Document *)
{
	m_has_exclusive_edit = false;
}


void MultiCursorView::deleteLinesWithCursor()
{
  std::vector<int> lines(m_cursors.size());
  auto pos = lines.begin();
  int last_line = -1;
  for (Cursor & c : m_cursors) {
    const int line = c.line();
    if (line != last_line) {
      last_line = line;
      *pos = line;
      ++pos;
    }
  }

  m_cursors.clear();
  stopCursors();

  auto e = lines.begin();
  while (pos != e) {
    m_document->removeLine(*--pos);
  }
}

void MultiCursorView::deleteWordLeft()
{
  std::size_t i = m_cursors.size();
  for (; i != 0; --i) {
    Cursor & c = m_cursors[i-1];
    if (c == m_view->cursorPosition()) {
      continue;
    }
    const std::size_t sz = m_cursors.size();
    m_document->removeText(KTextEditor::Range(
      CursorListDetail::wordPrev(m_document, c.line(), c.column())
    , c.cursor()
    ));
    i -= sz - m_cursors.size();
  }
}

void MultiCursorView::deleteWordRight()
{
  class RevalidCursor : public KTextEditor::MovingRangeFeedback {
    MultiCursorView & m_cursorview;
  public:
    Cursor * m_cursor;
    bool m_is_same;

    RevalidCursor(MultiCursorView & cursorview)
    : m_cursorview(cursorview)
    {}

    virtual void rangeEmpty(KTextEditor::MovingRange* range) {
      if (m_cursor->isSame(range)) {
        m_is_same = true;
      }
      else {
        m_cursorview.m_invalided_cursor.rangeEmpty(range);
      }
    }
  };

  RevalidCursor feedback(*this);

  for (Cursor & c : m_cursors) {
    c.setFeedback(&feedback);
  }
  for (std::size_t i = 0; i < m_cursors.size(); ++i) {
    Cursor & c = m_cursors[i];
    feedback.m_cursor = &c;
    feedback.m_is_same = false;
    const int line = c.line();
    const int column = c.column();
    m_document->removeText(KTextEditor::Range(
      KTextEditor::Cursor(line, column)
    , CursorListDetail::wordNext(m_document, line, column)
    ));
    if (feedback.m_is_same) {
      c.setCursor(line, column);
    }
  }
  for (Cursor & c : m_cursors) {
    c.setFeedback(&m_invalided_cursor);
  }
}

void MultiCursorView::backspace()
{
  for (std::size_t i = m_cursors.front().cursor().atStartOfDocument()
  ? 1 : 0; i != m_cursors.size(); ++i) {
    Cursor & c = m_cursors[i];
    if (c == m_view->cursorPosition()) {
      continue;
    }
    const std::size_t sz = m_cursors.size();
    const int line = c.line();
    const int column = c.column();
    int column2 = column;
    int line2 = line;
    if (!column) {
      column2 = m_document->lineLength(--line2);
    }
    else {
      --column2;
    }
    m_document->removeText(KTextEditor::Range(line, column, line2, column2));
    i -= sz - m_cursors.size();
  }
}

void MultiCursorView::deleteNextCharacter()
{
  if (startEditing()) {
    std::size_t i = m_cursors.size()
      - (m_cursors.front().cursor().atEndOfDocument() ? 1 : 0);
    while (i) {
      Cursor & c = m_cursors[--i];
      if (c == m_view->cursorPosition()) {
        continue;
      }
      const int line = c.line();
      const int column = c.column();
      int column2 = column;
      int line2 = line;
      if (column == m_document->lineLength(line)) {
        column2 = 0;
        ++line2;
      }
      else {
        ++column2;
      }
      m_document->removeText(KTextEditor::Range(line, column, line2, column2));
    }
    uniqueCont(m_cursors);
    for (Cursor & c : m_cursors) {
      c.setCursor(c.cursor());
    }
    endEditing();
  }
}

#define SIGNALMAN_OBJECT(O, F, P) F(O, SIGNAL(P), this, SLOT(P))
#define SIGNALMAN_DOC(F, P) SIGNALMAN_OBJECT(m_document, F, P)

#define SIGNALMAN_CHECK_VAR m_ranges
#define SIGNALMAN_CHECK(F) SIGNALMAN_CHECK_##F
#define SIGNALMAN_CHECK_connect SIGNALMAN_CHECK_VAR.empty()
#define SIGNALMAN_CHECK_disconnect !SIGNALMAN_CHECK_VAR.empty()
#define SIGNALMAN_F_TO_BOOL_connect true
#define SIGNALMAN_F_TO_BOOL_disconnect false

#define SIGNALMAN_CURSORS(F)                                                  \
  do {                                                                        \
    KActionCollection* collec = m_view->actionCollection();                   \
    F(                                                                        \
      collec->action("delete_line"), SIGNAL(triggered(bool)),                 \
      this, SLOT(deleteLinesWithCursor()));                                   \
    F(                                                                        \
      collec->action("delete_word_left"), SIGNAL(triggered(bool)),            \
      this, SLOT(deleteWordLeft()));                                          \
    F(                                                                        \
      collec->action("delete_word_right"), SIGNAL(triggered(bool)),           \
      this, SLOT(deleteWordRight()));                                         \
    F(                                                                        \
      collec->action("delete_next_character"), SIGNAL(triggered(bool)),       \
      this, SLOT(deleteNextCharacter()));                                     \
    F(                                                                        \
      collec->action("backspace"), SIGNAL(triggered(bool)),                   \
      this, SLOT(backspace()));                                               \
    SIGNALMAN_DOC(F,textInserted(KTextEditor::Document*,KTextEditor::Range)); \
    if (SIGNALMAN_CHECK(F)) {                                                 \
      SIGNALMAN_DOC(F, exclusiveEditStart(KTextEditor::Document*));           \
      SIGNALMAN_DOC(F, exclusiveEditEnd(KTextEditor::Document*));             \
    }                                                                         \
    actionCollection()->action("from_cursor_multiselection")                  \
      ->setEnabled(SIGNALMAN_F_TO_BOOL_##F);                                  \
  } while (0)

void MultiCursorView::connectCursors()
{
  SIGNALMAN_CURSORS(connect);
}

void MultiCursorView::disconnectCursors()
{
  SIGNALMAN_CURSORS(disconnect);
  if (m_is_synchronized_cursor) {
    actionCollection()->action("synchronise_multicursor")->trigger();
  }
}

#undef SIGNALMAN_CURSORS
#undef SIGNALMAN_CHECK_VAR
#undef SIGNALMAN_F_TO_BOOL_connect
#undef SIGNALMAN_F_TO_BOOL_disconnect

#define SIGNALMAN_CURSORS_SYNCHRONIZE(F)                                \
  KActionCollection* collec = m_view->actionCollection();               \
  do {                                                                  \
    F(                                                                  \
      collec->action("move_line_up"), SIGNAL(triggered(bool)),          \
      this, SLOT(moveCursorToUp()));                                    \
    F(                                                                  \
      collec->action("move_line_down"), SIGNAL(triggered(bool)),        \
      this, SLOT(moveCursorToDown()));                                  \
    F(                                                                  \
      /* "cusor" is ok */                                               \
      collec->action("move_cusor_left"), SIGNAL(triggered(bool)),       \
      this, SLOT(moveCursorToLeft()));                                  \
    F(                                                                  \
      collec->action("move_cursor_right"), SIGNAL(triggered(bool)),     \
      this, SLOT(moveCursorToRight()));                                 \
    F(                                                                  \
      collec->action("beginning_of_line"), SIGNAL(triggered(bool)),     \
      this, SLOT(moveCursorToBeginningOfLine()));                       \
    F(                                                                  \
      collec->action("end_of_line"), SIGNAL(triggered(bool)),           \
      this, SLOT(moveCursorToEndOfLine()));                             \
    F(                                                                  \
      collec->action("to_matching_bracket"), SIGNAL(triggered(bool)),   \
      this, SLOT(moveCursorToMatchingBracket()));                       \
    F(                                                                  \
      collec->action("word_left"), SIGNAL(triggered(bool)),             \
      this, SLOT(moveCursorToWordLeft()));                              \
    F(                                                                  \
      collec->action("word_right"), SIGNAL(triggered(bool)),            \
      this, SLOT(moveCursorToWordRight()));                             \
  } while(0)

void MultiCursorView::connectSynchronizedCursors()
{
  SIGNALMAN_CURSORS_SYNCHRONIZE(connect);
}

void MultiCursorView::disconnectSynchronizedCursors()
{
  SIGNALMAN_CURSORS_SYNCHRONIZE(disconnect);
}

#undef SIGNALMAN_CURSORS_SYNCHRONIZE

void MultiCursorView::setSynchronizedCursors()
{
  if (m_is_synchronized_cursor) {
    m_is_synchronized_cursor = false;
    disconnectSynchronizedCursors();
    for (Cursor & c : m_cursors) {
      c.resetkeepedColumn();
    }
  } else {
    m_is_synchronized_cursor = true;
    connectSynchronizedCursors();
  }
}

#define SIGNALMAN_OBJECT(O, F, P) F(O, SIGNAL(P), this, SLOT(P))
#define SIGNALMAN_DOC(F, P) SIGNALMAN_OBJECT(m_document, F, P)

#define SIGNALMAN_CHECK_VAR m_cursors

#define SIGNALMAN_RANGES(F)\
  do { \
    if (SIGNALMAN_CHECK(F)) {\
      SIGNALMAN_DOC(F, exclusiveEditStart(KTextEditor::Document*));\
      SIGNALMAN_DOC(F, exclusiveEditEnd(KTextEditor::Document*));\
    }\
  } while (0)

void MultiCursorView::connectRanges()
{
  SIGNALMAN_RANGES(connect);
}

void MultiCursorView::disconnectRanges()
{
  SIGNALMAN_RANGES(disconnect);
  if (m_is_synchronized_selection) {
    actionCollection()->action("synchronise_multiselection")->trigger();
  }
}

#undef SIGNALMAN_RANGES
#undef SIGNALMAN_CHECK
#undef SIGNALMAN_CHECK_connect
#undef SIGNALMAN_CHECK_disconnect
#undef SIGNALMAN_CHECK_VAR
#undef SIGNALMAN_OBJECT
#undef SIGNALMAN_DOC

#define SIGNALMAN_RANGES_SYNCHRONIZE(F)                                    \
  do{                                                                      \
    KActionCollection* collec = m_view->actionCollection();                \
    F(                                                                     \
      collec->action("select_line_up"), SIGNAL(triggered(bool)),           \
      this, SLOT(selectLineUp()));                                         \
    F(                                                                     \
      collec->action("select_line_down"), SIGNAL(triggered(bool)),         \
      this, SLOT(selectLineDown()));                                       \
    F(                                                                     \
      collec->action("select_char_left"), SIGNAL(triggered(bool)),         \
      this, SLOT(selectCharLeft()));                                       \
    F(                                                                     \
      collec->action("select_char_right"), SIGNAL(triggered(bool)),        \
      this, SLOT(selectCharRight()));                                      \
    F(                                                                     \
      collec->action("select_word_left"), SIGNAL(triggered(bool)),         \
      this, SLOT(selectWordLeft()));                                       \
    F(                                                                     \
      collec->action("select_word_right"), SIGNAL(triggered(bool)),        \
      this, SLOT(selectWordRight()));                                      \
    F(                                                                     \
      collec->action("select_beginning_of_line"), SIGNAL(triggered(bool)), \
      this, SLOT(selectBeginningOfLine()));                                \
    F(                                                                     \
      collec->action("select_end_of_line"), SIGNAL(triggered(bool)),       \
      this, SLOT(selectEndOfLine()));                                      \
    F(                                                                     \
      collec->action("select_matching_bracket"), SIGNAL(triggered(bool)),  \
      this, SLOT(selectMatchingBracket()));                                \
    /*F(                                                                   \
      collec->action("select_page_up"), SIGNAL(triggered(bool)),           \
      this, SLOT(selectPageUp()));                                         \
    F(                                                                     \
      collec->action("select_page_down"), SIGNAL(triggered(bool)),         \
      this, SLOT(selectPageDown()));*/                                     \
  } while(0)

void MultiCursorView::connectSynchronizedRanges()
{
  SIGNALMAN_RANGES_SYNCHRONIZE(connect);
}

void MultiCursorView::disconnectSynchronizedRanges()
{
  SIGNALMAN_RANGES_SYNCHRONIZE(disconnect);
}

void MultiCursorView::setSynchronizedRanges()
{
  if (m_is_synchronized_selection) {
    m_is_synchronized_selection = false;
    disconnectSynchronizedRanges();
  } else {
    m_is_synchronized_selection = true;
    connectSynchronizedRanges();
  }
}

void MultiCursorView::stopCursors()
{
  disconnectCursors();
  setEnabledCursors(false);
}

void MultiCursorView::startCursors()
{
  connectCursors();
  setEnabledCursors(true);
}

void MultiCursorView::checkCursors()
{
  if (m_cursors.empty()) {
    stopCursors();
  }
}

void MultiCursorView::stopRanges()
{
  disconnectRanges();
  setEnabledRanges(false);
}

void MultiCursorView::startRanges()
{
  connectRanges();
  setEnabledRanges(true);
}

void MultiCursorView::checkRanges()
{
  if (m_ranges.empty()) {
    stopRanges();
  }
}

void MultiCursorView::setEnabledRanges(bool x)
{
  KActionCollection * collec = actionCollection();
  // TODO enable/disable submenu
  collec->action("remove_all_multiselection")->setEnabled(x);
  collec->action("remove_line_multiselection")->setEnabled(x);
  collec->action("clear_multiselection")->setEnabled(x);
  collec->action("copy_multiselection")->setEnabled(x);
  collec->action("cut_multiselection")->setEnabled(x);
  collec->action("paste_multiselection")->setEnabled(x);
  collec->action("next_start_multiselection")->setEnabled(x);
  collec->action("previous_start_multiselection")->setEnabled(x);
  collec->action("next_end_multiselection")->setEnabled(x);
  collec->action("previous_end_multiselection")->setEnabled(x);
}

void MultiCursorView::setEnabledCursors(bool x)
{
  KActionCollection * collec = actionCollection();
  // TODO enable/disable submenu
  collec->action("backspace_multicursor")->setEnabled(x);
  collec->action("delete_multicursor")->setEnabled(x);
  collec->action("remove_all_multicursor")->setEnabled(x);
  collec->action("remove_cursor_line_multicursor")->setEnabled(x);
  collec->action("next_multicursor")->setEnabled(x);
  collec->action("previous_multicursor")->setEnabled(x);
  collec->action("synchronise_multicursor")->setEnabled(x);
  collec->action("copy_line_with_cursor")->setEnabled(x);
  collec->action("cut_line_with_cursor")->setEnabled(x);
  collec->action("paste_line_with_cursor")->setEnabled(x);
  collec->action("extend_left_selection")->setEnabled(x);
  collec->action("extend_right_selection")->setEnabled(x);
  collec->action("reduce_left_selection")->setEnabled(x);
  collec->action("reduce_right_selection")->setEnabled(x);
}

void MultiCursorView::moveCursorToUp()
{
  auto first = std::find_if(m_cursors.begin(), m_cursors.end()
  , [](Cursor const & c) { return c.line() > 0; });
  auto cpfirst = m_cursors.begin();
  auto end = m_cursors.end();
  for (; first != end; ++first, ++cpfirst) {
    const int line = first->line() - 1;
    const int column = first->column();
    if (column < first->getKeepedColumn()) {
      const int line_len = m_document->lineLength(line);
      cpfirst->setCursor(line, qMin(line_len, first->getKeepedColumn()));
    }
    else {
      cpfirst->setCursorAndKeepColumn(line, column);
    }
  }
  m_cursors.erase(std::unique(m_cursors.begin(), cpfirst), end);
  checkCursors();
}

void MultiCursorView::moveCursorToDown()
{
  auto first = m_cursors.begin();
  auto end = m_cursors.end();
  const int lmax = m_document->lines() - 1;
  for (; first != end; ++first) {
    if (lmax < first->line()) {
      break;
    }
    const int column = first->column();
    const int line = first->line() + 1;
    if (column < first->getKeepedColumn()) {
      const int line_len = m_document->lineLength(line);
      first->setCursor(line, qMin(line_len, first->getKeepedColumn()));
    }
    else {
      first->setCursorAndKeepColumn(line, column);
    }
  }
  m_cursors.erase(std::unique(m_cursors.begin(), first), end);
  checkCursors();
}

void MultiCursorView::moveCursorToLeft()
{
  auto first = m_cursors.begin();
  if (m_cursors.front().line() == 0 && m_cursors.front().column() == 0) {
    ++first;
  }
  CursorListDetail::moveCursor(*this, first
  , [this](Cursor const & cur) {
    const int l = cur.line();
    const int c = cur.column();
    if (c == 0) {
      return KTextEditor::Cursor(l-1, m_document->lineLength(l));
    }
    return KTextEditor::Cursor(l, c-1);
  });
}

void MultiCursorView::moveCursorToRight()
{
  auto first = m_cursors.rbegin();
  if (m_cursors.back() == m_document->documentEnd()) {
    ++first;
  }
  auto end = m_cursors.rend();
  if (first != end) {
    m_cursors.erase(
      m_cursors.begin()
    , CursorListDetail::moveCursorImpl(
        first, end
      , m_cursors.rbegin()
      , [this](Cursor const & cur) {
        const int l = cur.line();
        const int c = cur.column();
        if (m_document->lineLength(l) == c) {
          return KTextEditor::Cursor(l+1, 0);
        }
        return KTextEditor::Cursor(l, c+1);
    }).base());
  }
  else {
    m_cursors.clear();
    stopCursors();
  }
}

void MultiCursorView::moveCursorToBeginningOfLine()
{
  for (Cursor & c : m_cursors) {
    c.setCursor(KTextEditor::Cursor(c.line(), 0));
  }
  uniqueCont(m_cursors);
}

void MultiCursorView::moveCursorToEndOfLine()
{
  for (Cursor & c : m_cursors) {
    const int l = c.line();
    c.setCursor(KTextEditor::Cursor(l, m_document->lineLength(l)));
  }
  uniqueCont(m_cursors);
}

void MultiCursorView::moveCursorToWordLeft()
{
  for (Cursor & c : m_cursors) {
    c.setCursor(CursorListDetail::wordPrev(m_document, c.line(), c.column()));
  }
  uniqueCont(m_cursors);
}

void MultiCursorView::moveCursorToWordRight()
{
  for (Cursor & c : m_cursors) {
    c.setCursor(CursorListDetail::wordNext(m_document, c.line(), c.column()));
  }
  uniqueCont(m_cursors);
}

void MultiCursorView::moveCursorToMatchingBracket()
{
  KTextEditor::HighlightInterface *iface
    = qobject_cast<KTextEditor::HighlightInterface*>(m_document);
  if (!iface) {
    iface = &CursorListDetail::fake_highlight();
  }
  for (Cursor & c : m_cursors) {
    KTextEditor::Cursor cursor = c.cursor();
    if (CursorListDetail::matchingBracket(m_document, iface, cursor)) {
      c.setCursor(cursor);
    }
  }
  uniqueCont(m_cursors);
}

void MultiCursorView::setCursor(const KTextEditor::Cursor& cursor)
{
  auto it = lowerBound(m_cursors, cursor);
  if (it != m_cursors.end() && *it == cursor) {
    m_cursors.erase(it);
    checkCursors();
  }
  else {
    auto moving_cursor = newMovingCursor(cursor);
    if (m_cursors.empty()) {
      m_cursors.emplace_back(moving_cursor);
      startCursors();
    }
    else {
      m_cursors.emplace(it, moving_cursor);
    }
  }
}

void MultiCursorView::rangesFromCursors()
{
  for (auto & c : m_cursors) {
    KTextEditor::Cursor cursor = c.cursor();
    auto it_start = CursorListDetail::lowerBoundEnd(m_ranges, cursor);
    if (it_start == m_ranges.end() || it_start->end() < cursor) {
      KTextEditor::Range range(cursor, cursor);
      m_ranges.emplace(it_start, newMovingRange(range));
    }
  }
}

void MultiCursorView::selectLineUp()
{
  CursorListDetail::selectAlgoLeft(*this
  , [this](int line, int column) {
    return KTextEditor::Cursor(qMax(line - 1, 0), column);
  });
}

void MultiCursorView::selectLineDown()
{
  CursorListDetail::selectAlgoRight(*this
  , [this](int line, int column) {
    if (line + 1 < m_document->lines()) {
      ++line;
    }
    return KTextEditor::Cursor(line, column);
  });
}

void MultiCursorView::selectCharRight()
{
  const int linemax = m_document->lines();
  CursorListDetail::selectAlgoRight(*this
  , [linemax, this](int line, int column) {
    return (column + 1 < m_document->lineLength(line))
      ? KTextEditor::Cursor(line, column + 1)
      : ((linemax != line + 1)
        ? KTextEditor::Cursor(line + 1, 0)
        : KTextEditor::Cursor(line, column)
      );
  });
}

void MultiCursorView::selectCharLeft()
{
  CursorListDetail::selectAlgoLeft(*this
  , [this](int line, int column) {
    return (column != 0)
      ? KTextEditor::Cursor(line, column - 1)
      : ((line != 0)
        ? KTextEditor::Cursor(line - 1, m_document->lineLength(line - 1))
        : KTextEditor::Cursor(line, column)
      );
  });
}

void MultiCursorView::selectBeginningOfLine()
{
  m_ranges_temp.swap(m_ranges);
  m_ranges.clear();
  m_ranges.reserve(m_ranges_temp.size());
  for (Range & r : m_ranges_temp) {
    KTextEditor::Cursor c(r.start().line(), 0);
    setRange(KTextEditor::Range(c, r.end()), false);
  }
  m_ranges_temp.clear();
}

void MultiCursorView::selectEndOfLine()
{
  m_ranges_temp.swap(m_ranges);
  m_ranges.clear();
  m_ranges.reserve(m_ranges_temp.size());
  for (Range & r : m_ranges_temp) {
    const int line = r.end().line();
    const int column = m_document->lineLength(line);
    KTextEditor::Cursor c(line, column);
    setRange(KTextEditor::Range(r.start(), c), false);
  }
  m_ranges_temp.clear();
}

void MultiCursorView::selectWordRight()
{
  CursorListDetail::selectAlgoRight(*this
  , [this](int line, int column) {
    return CursorListDetail::wordNext(m_document, line, column);
  });
}

void MultiCursorView::selectWordLeft()
{
  CursorListDetail::selectAlgoLeft(*this
  , [this](int line, int column) {
    return CursorListDetail::wordPrev(m_document, line, column);
  });
}

//void MultiCursorView::selectPageUp()
//{
//// TODO
//}

//void MultiCursorView::selectPageDown()
//{
//// TODO
//}

void MultiCursorView::selectMatchingBracket()
{
  KTextEditor::HighlightInterface *iface
    = qobject_cast<KTextEditor::HighlightInterface*>(m_document);
  if (!iface) {
    iface = &CursorListDetail::fake_highlight();
  }
  CursorListDetail::selectAlgoLeft(*this
  , [iface, this](int line, int column) {
    KTextEditor::Cursor cursor(line, column);
    if (CursorListDetail::matchingBracket(m_document, iface, cursor)) {
      return cursor;
    }
    return KTextEditor::Cursor(line, column);
  });
}

void MultiCursorView::setRange(
  const KTextEditor::Range& range, bool remove_if_contains)
{
  if (m_ranges.empty()) {
    startRanges();
  }

  auto it_start = CursorListDetail::lowerBoundEnd(m_ranges, range.start());

  if (it_start == m_ranges.end()) {
    m_ranges.emplace_back(newMovingRange(range));
    return ;
  }

  auto it_end = std::lower_bound(it_start, m_ranges.end(), range.end()
  , [](Range const & r, KTextEditor::Cursor const & c){
      return r.start() < c;
    }
  );
  if (it_start == it_end) {
    if (it_start->start() == range.end()) {
      it_start->setRange(range.start(), it_start->end());
    }
    else {
      m_ranges.emplace(it_start, newMovingRange(range));
    }
  }
  else {
    if (it_start + 1 == it_end && it_start->contains(range)) {
      if (!remove_if_contains) {
        return ;
      }
      removeRange(it_start, range);
    }
    else {
      it_start->setRange(
        qMin(it_start->start().toCursor(), range.start()),
        qMax((it_end-1)->end().toCursor(), range.end())
      );
      m_ranges.erase(++it_start, it_end);
    }
  }
}

void MultiCursorView::removeRange(
  RangeList::iterator it, const KTextEditor::Range& range)
{
  const KTextEditor::Range rightrange(range.end(), it->end());
  const KTextEditor::Range leftrange(it->start(), range.start());
  if (!rightrange.isEmpty()) {
    if (leftrange.isEmpty()) {
      it->setRange(rightrange);
    }
    else {
      it->setRange(leftrange.start(), range.start());
      m_ranges.emplace(it+1, newMovingRange(rightrange));
    }
  }
  else if (leftrange.isEmpty()) {
    m_ranges.erase(it);
    checkRanges();
  }
  else {
    it->setRange(leftrange.start(), range.start());
  }
}

void MultiCursorView::setCursor()
{
	if (m_view->selection()) {
		const KTextEditor::Range& range = m_view->selectionRange();
		for (int line = range.start().line(); line != range.end().line() + 1; ++line) {
			setCursor(KTextEditor::Cursor(line, qMin(range.start().column(), m_document->lineLength(line))));
		}
	} else {
		setCursor(m_view->cursorPosition());
	}
}

void MultiCursorView::setActiveCursorCtrlClick(
  bool active, bool remove_cursor_if_only_click)
{
  m_remove_cursor_if_only_click = remove_cursor_if_only_click;
  setEventFilter(m_has_cursor_ctrl, active);
}

void MultiCursorView::setActiveSelectionCtrlClick(bool active)
{
  setEventFilter(m_has_selection_ctrl, active);
}

void MultiCursorView::setActiveRemoveAllIfEsc(bool active)
{
  setEventFilter(m_remove_all_if_esc, active);
}

void MultiCursorView::setEventFilter(bool & r, bool x)
{
  bool is_active = m_has_selection_ctrl || m_has_cursor_ctrl || m_remove_all_if_esc;
  r = x;
  if (is_active == (
    m_has_selection_ctrl || m_has_cursor_ctrl || m_remove_all_if_esc
  )) {
    return ;
  }

  if (x) {
    m_view->focusProxy()->installEventFilter(this);
  }
  else {
    m_view->focusProxy()->removeEventFilter(this);
  }
}

bool MultiCursorView::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyRelease) {
    if (m_remove_all_if_esc
     && not QApplication::keyboardModifiers()
     && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape
     && not m_view->selection()) {
      removeAllCursors();
      removeAllRanges();
      return false;
    }
  }
  else if (event->type() == QEvent::MouseButtonRelease) {
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
      if (m_view->selection()) {
        if (m_has_selection_ctrl) {
          // TODO synchronized
          setRange(m_view->selectionRange());
          return false;
        }
      }
      else {
        if (m_has_cursor_ctrl) {
          setCursor(m_view->cursorPosition());
          return false;
        }
      }
    }
    else if (m_remove_cursor_if_only_click) {
      removeAllCursors();
      return false;
    }
  }
  return QObject::eventFilter(obj, event);
}

void MultiCursorView::textInserted(KTextEditor::Document *doc, const KTextEditor::Range &range)
{
	if (startEditing()) {
		const QString text = doc->text(range);
    auto it = lowerBound(m_cursors, m_view->cursorPosition());
    for (auto first = m_cursors.begin(); first != it; ++first) {
      m_document->insertText(first->cursor(), text);
    }
    auto last = m_cursors.end();
    if (it != last) {
      if (m_view->cursorPosition() != it->cursor()) {
        m_document->insertText(it->cursor(), text);
      }
      while (++it != last) {
        m_document->insertText(it->cursor(), text);
      }
    }
		endEditing();
	}
}

void MultiCursorView::removeAllCursors()
{
  if (m_view->selection()) {
    const KTextEditor::Range& range = m_view->selectionRange();
    auto first = lowerBound(m_cursors, range.start());
    auto last = std::upper_bound(first, m_cursors.end(), range.start());
    m_cursors.erase(first, last);
    if (m_cursors.empty()) {
      stopCursors();
    }
  } else {
    m_cursors.clear();
    stopCursors();
  }
}

void MultiCursorView::removeCursorsOnLine()
{
  const int line = m_view->cursorPosition().line();
  auto first = lowerBound(m_cursors, line
  , [](Cursor const & c, int line) { return c.line() < line; });
  if (first != m_cursors.end() && first->line() == line){
    auto last = std::upper_bound(
      first, m_cursors.end(), line
    , [](int line, Cursor const & c) { return line < c.line(); });
    m_cursors.erase(first, last);
    if (m_cursors.empty()) {
      stopCursors();
    }
  }
}

void MultiCursorView::moveToNextCursor()
{
  CursorListDetail::moveToNext(
    m_cursors, m_view, CursorListDetail::ProxyCursor());
}

void MultiCursorView::moveToPreviousCursor()
{
  CursorListDetail::moveToPrevious(
    m_cursors, m_view, CursorListDetail::ProxyCursor());
}

void MultiCursorView::setActiveCursor()
{
	if (m_is_active) {
		m_is_active = false;
		disconnectCursors();
	} else {
		m_is_active = true;
		connectCursors();
	}
}

void MultiCursorView::clearRanges()
{
  if (startEditing(false)) {
    std::for_each(m_ranges.rbegin(), m_ranges.rend()
    , [this](Range const & r){ m_document->removeText(r.toRange()); });
    removeAllRanges();
    endEditing();
  }
}

void MultiCursorView::copyRanges()
{
  int l = m_ranges.front().end().line();
  QString s(m_document->text(m_ranges.front().toRange()));
  std::for_each(m_ranges.cbegin()+1, m_ranges.cend(), [&](Range const & r) {
    if (r.isEmpty()) {
      return ;
    }
    const KTextEditor::Range range = r.toRange();
    s.append(range.start().line() != l ? '\n' : ' ');
    l = range.end().line();
    s.append(m_document->text(range));
  });
  QApplication::clipboard()->setText(s);
}

void MultiCursorView::cutRanges()
{
  copyRanges();
  clearRanges();
}

void MultiCursorView::pasteRanges()
{
  if (startEditing(false)) {
    const QString text = QApplication::clipboard()->text();
    std::for_each(m_ranges.rbegin(), m_ranges.rend()
    , [this, &text](Range const & r){
      if (r.isEmpty()) {
        return ;
      }
      KTextEditor::Range range = r.toRange();
      m_document->insertText(r.end(), text);
      m_document->removeText(range);
    });
    endEditing();
  }
}

void MultiCursorView::setRange()
{
  if (m_view->selection()) {
    const KTextEditor::Range & range = m_view->selectionRange();

    if (m_view->blockSelection()) {
      int column_start = range.start().column();
      int column_end = range.end().column();

      if (column_start == column_end) {
        return ;
      }

      if (column_end < column_start) {
        using std::swap;
        swap(column_end, column_start);
      }

      const int line_start = range.start().line();
      const int line_end = range.end().line();

      int line = line_start;

      for (; line <= line_end; ++line) {
        auto it_start = CursorListDetail::lowerBoundEnd(
          m_ranges, KTextEditor::Cursor(line, column_start));
        if (it_start == m_ranges.end()
         || !it_start->contains(KTextEditor::Range(
           line, column_start, line, column_end))) {
          break;
        }
      }

      if (line > line_end) {
        for (line = line_start; line <= line_end; ++line) {
          removeRange(
            CursorListDetail::lowerBoundEnd(
              m_ranges, KTextEditor::Cursor(line, column_start)),
            KTextEditor::Range(line, column_start, line, column_end)
          );
        }
      }
      else {
        for (; line <= line_end; ++line) {
          KTextEditor::Range range(line, column_start, line, column_end);
          setRange(range, false);
        }
      }
    }
    else {
      setRange(range);
    }
  }
  else {
    KTextEditor::Cursor cursor = m_view->cursorPosition();
    auto it = CursorListDetail::lowerBoundEnd(m_ranges, cursor);
    if (it != m_ranges.end() && it->start() <= cursor) {
      m_ranges.erase(it);
      checkRanges();
    }
  }
}

void MultiCursorView::copyLinesWithCursor()
{
  int l = -1;
  QString s;
  for(Cursor const & c : m_cursors) {
    const int l2 = c.line();
    if (l != l2) {
      l = l2;
      s.append(m_document->line(l));
      s.append('\n');
    }
  };
  QApplication::clipboard()->setText(s);
}

void MultiCursorView::cutLinesWithCursor()
{
  copyLinesWithCursor();
  if (startEditing(false)) {
    stopCursors();
    int l = m_cursors.back().line();
    std::for_each(m_cursors.rbegin()+1, m_cursors.rend(), [&](Cursor const & c) {
      const int l2 = c.line();
      if (l != l2) {
        m_document->removeLine(l);
        l = l2;
      }
    });
    m_document->removeLine(l);
    m_cursors.clear();
    endEditing();
  }
}

void MultiCursorView::pasteLinesOnCursors()
{
  if (startEditing(false)) {
    QString s = QApplication::clipboard()->text();
    if (!s.isEmpty()) {
      int i = 0;
      for (Cursor & c : m_cursors) {
        const int i2 = s.indexOf('\n', i);
        m_document->insertText(c.cursor(), s.mid(i, i2-i));
        i = i2 + 1;
        if (i2 == -1) {
          break;
        }
      }
    }
    endEditing();
  }
}

void MultiCursorView::extendLeftSelection()
{
  if (m_view->selection()) {
    const KTextEditor::Range & range = m_view->selectionRange();
    auto it = lowerBound(m_cursors, range.start());
    if (it != m_cursors.begin() && *--it < range.start()) {
      m_view->setSelection(KTextEditor::Range(it->cursor(), range.end()));
    }
  }
  else {
    const KTextEditor::Cursor cursor = m_view->cursorPosition();
    auto it = lowerBound(m_cursors, cursor);
    if (it != m_cursors.begin() && *--it < cursor) {
      m_view->setSelection(KTextEditor::Range(it->cursor(), cursor));
    }
  }
}

void MultiCursorView::extendRightSelection()
{
  if (m_view->selection()) {
    const KTextEditor::Range & range = m_view->selectionRange();
    auto it = lowerBound(m_cursors, range.end());
    if (it != m_cursors.end()) {
      m_view->setSelection(KTextEditor::Range(range.start(), it->cursor()));
    }
  }
  else {
    const KTextEditor::Cursor cursor = m_view->cursorPosition();
    auto it = lowerBound(m_cursors, cursor);
    if (it != m_cursors.end()) {
      m_view->setSelection(KTextEditor::Range(cursor, it->cursor()));
    }
  }
}

void MultiCursorView::reduceLeftSelection()
{
  if (m_view->selection()) {
    const KTextEditor::Range & range = m_view->selectionRange();
    auto it = lowerBound(m_cursors, range.start());
    if (it == m_cursors.end() || !(*it < range.end())) {
      m_view->removeSelection();
    }
    else {
      m_view->setSelection(KTextEditor::Range(it->cursor(), range.end()));
    }
  }
}

void MultiCursorView::reduceRightSelection()
{
  if (m_view->selection()) {
    const KTextEditor::Range & range = m_view->selectionRange();
    auto it = lowerBound(m_cursors, range.end());
    if (it == m_cursors.end() || !(range.start() < *it)) {
      m_view->removeSelection();
    }
    else {
      m_view->setSelection(KTextEditor::Range(range.start(), it->cursor()));
    }
  }
}

void MultiCursorView::moveToNextEndRange()
{
  CursorListDetail::moveToNext(
    m_ranges, m_view, CursorListDetail::RangeEnd());
}

void MultiCursorView::moveToNextStartRange()
{
  CursorListDetail::moveToNext(
    m_ranges, m_view, CursorListDetail::RangeStart());
}

void MultiCursorView::moveToPreviousEndRange()
{
  CursorListDetail::moveToPrevious(
    m_ranges, m_view, CursorListDetail::RangeEnd());
}

void MultiCursorView::moveToPreviousStartRange()
{
  CursorListDetail::moveToPrevious(
    m_ranges, m_view, CursorListDetail::RangeStart());
}

void MultiCursorView::removeAllRanges()
{
  m_ranges.clear();
  stopRanges();
}

void MultiCursorView::removeRangesOnline()
{
  const int line = m_view->cursorPosition().line();
  auto it_start = lowerBound(m_ranges, line
  , [](Range const & r, int l){
      return r.end().line() < l;
    }
  );
  if (it_start != m_ranges.end()) {
    if (it_start->start().line() < line && it_start->end().line() > line) {
      auto moving_range = newMovingRange(KTextEditor::Range(
        KTextEditor::Cursor(line+1, 0), it_start->end()
      ));
      it_start->setRange(
        it_start->start(),
        KTextEditor::Cursor(line-1, m_document->lineLength(line-1))
      );
      m_ranges.emplace(it_start+1, moving_range);
    }
    else if (it_start->end().line() > line) {
      it_start->setRange(
        KTextEditor::Cursor(line+1, 0),
        it_start->end()
      );
    }
    else {
      auto it_end = std::upper_bound(it_start, m_ranges.end(), line
        , [](int l, Range const & r){
          return l < r.start().line();
        }
      );
      if (it_start == it_end) {
        m_ranges.erase(it_start);
        checkRanges();
      }
      else {
        if (it_start->start().line() < line) {
          it_start->setRange(
            it_start->start(),
            KTextEditor::Cursor(line-1, m_document->lineLength(line-1))
          );
          ++it_start;
        }
        if (it_start != it_end) {
          if ((it_end-1)->end().line() > line) {
            KTextEditor::Cursor cursor(line+1, 0);
            if (cursor != (it_end-1)->end()) {
              --it_end;
              it_end->setRange(cursor, it_end->end());
            }
          }
          m_ranges.erase(it_start, it_end);
          checkRanges();
        }
      }
    }
  }
}

bool MultiCursorView::startEditing(bool check_active)
{
  if ((check_active && !m_is_active)
   || m_has_exclusive_edit
   || !m_document->startEditing()) {
    return false;
  }
  return m_has_exclusive_edit = true;
}

bool MultiCursorView::endEditing()
{
  m_has_exclusive_edit = false;
  return m_document->endEditing();
}

KTextEditor::MovingRange* MultiCursorView::newMovingCursor(
  const KTextEditor::Cursor& cursor) const
{
  KTextEditor::MovingRange * moving_range = m_smart->newMovingRange(
    KTextEditor::Range(cursor, cursor.line(), cursor.column() + 1));
  moving_range->setAttribute(m_cursor_attr);
  moving_range->setFeedback(
    const_cast<InvalidedCursor*>(&m_invalided_cursor));
  return moving_range;
}

KTextEditor::MovingRange* MultiCursorView::newMovingRange(
  const KTextEditor::Range& range) const
{
  KTextEditor::MovingRange * moving_range = m_smart->newMovingRange(range);
  moving_range->setAttribute(m_selection_attr);
  moving_range->setFeedback(
    const_cast<InvalidedRange*>(&m_invalided_range));
  return moving_range;
}

#include "multicursorview.moc"
