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

#include <KAction>
#include <KActionCollection>

#include <QApplication>
#include <QClipboard>
#include <KConfigGroup>

template<class Cont, class T>
static typename Cont::iterator
lowerBound(Cont & cont, const T & x)
{ return std::lower_bound(cont.begin(), cont.end(), x); }

template<class Cont, class T, class Compare>
static typename Cont::iterator
lowerBound(Cont & cont, const T & x, Compare comp)
{ return std::lower_bound(cont.begin(), cont.end(), x, comp); }


struct MultiCursorView::CursorListDetail
{
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

  static void moveCursorsByAction(
    const char * name, bool & is_moved,
    KTextEditor::View * view, CursorList & cursors, bool is_sorted = true)
  {
    if (is_moved) {
      return ;
    }
    is_moved = true;
    QAction * action = view->actionCollection()->action(name);
    KTextEditor::Cursor cursor = view->cursorPosition();
    for (Cursor & c : cursors) {
      view->setCursorPosition(c.cursor());
      action->trigger();
      c.setCursor(view->cursorPosition());
    }
    view->setCursorPosition(cursor);
    if (!is_sorted) {
      std::sort(cursors.begin(), cursors.end());
    }
    cursors.erase(std::unique(cursors.begin(), cursors.end()), cursors.end());
    is_moved = false;
  }

  static KTextEditor::Cursor
  recoil(
    KTextEditor::Document* doc,
    const KTextEditor::Cursor& cursor,
    int length, int minline = 0
  ) {
    if (cursor.column() >= length) {
      return KTextEditor::Cursor(cursor.line(), cursor.column() - length);
    }
    KTextEditor::Cursor ret(cursor.line() - 1, 0);
    length -= cursor.column();
    while (length && ret.line() >= minline) {
      const int line_length = doc->lineLength(ret.line());
      if (line_length < length) {
        ret.setLine(ret.line() - 1);
        length -= line_length - 1;
      }
      else {
        ret.setColumn(line_length - length + 1);
        length = 0;
      }
    }
    return ret;
  }

  static KTextEditor::Cursor
  advance(
    KTextEditor::Document* doc,
    const KTextEditor::Cursor& cursor,
    int length, int endline
  ) {
    KTextEditor::Cursor ret(cursor);
    int line_length = doc->lineLength(cursor.line());
    if (ret.column() + length - 1 < line_length)
    ret.setColumn(ret.column() + length);
    else
    {
      length -= line_length - ret.column() + 1;
      ret.setColumn(0);
      ret.setLine(ret.line() + 1);
      while (length && ret.line() < endline)
      {
        line_length = doc->lineLength(ret.line());
        if (line_length < length)
        {
          ret.setLine(ret.line() + 1);
          length -= line_length - 1;
        }
        else
        {
          ret.setColumn(length);
          length = 0;
        }
      }
    }
    return ret;
  }

  static void removeForwardText(
    KTextEditor::Document* doc, CursorList & cursors, int length)
  {
    CursorList::reverse_iterator last = cursors.rend();
    CursorList::reverse_iterator it = cursors.rbegin();
    KTextEditor::Cursor cursor
      = advance(doc, it->cursor(), length, doc->lines()+1);
    doc->removeText(KTextEditor::Range(it->cursor(), cursor));
    CursorList::reverse_iterator prev = it;
    while (++it != last) {
      cursor = advance(doc, it->cursor(), length, prev->cursor().line()+1);
      if (prev->cursor() <= cursor) {
        doc->removeText(KTextEditor::Range(it->cursor(), prev->cursor()));
        prev = CursorList::reverse_iterator(cursors.erase(it.base()));
        it = prev;
      } else {
        doc->removeText(KTextEditor::Range(it->cursor(), cursor));
        ++prev;
      }
    }
    for (Cursor & c : cursors) {
      c.revalid();
    }
  }

  template<class Pred>
  static void removeBackwardText(
    KTextEditor::Document* doc, CursorList & cursors, int length, Pred pred)
  {
    CursorList::iterator first = cursors.begin();
    CursorList::iterator last = cursors.end();
    if (pred(*first)) {
      doc->removeText(KTextEditor::Range(
        first->cursor(), recoil(doc, first->cursor(), length)
      ));
    }
    KTextEditor::Cursor cursor_prev = first->cursor();
    while (++first != last) {
      KTextEditor::Cursor cursor
        = recoil(doc, first->cursor(), length, cursor_prev.line());
      if (cursor_prev >= cursor) {
        if (pred(*first)) {
          doc->removeText(KTextEditor::Range(first->cursor(), cursor_prev));
        }
        first = cursors.erase(--first);
        last = cursors.end();
        cursor_prev = cursor;
      } else if (pred(*first)) {
        doc->removeText(KTextEditor::Range(first->cursor(), cursor));
      }
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

  ENTRY("Backspace Character on Virtuals Cursors", "backspace_multicursor", textBackspace());
	action->setShortcut(Qt::ALT + Qt::Key_Backspace);

  ENTRY("Delete Character on Virtuals Cursors", "delete_multicursor", textDelete());
	action->setShortcut(Qt::ALT + Qt::Key_Delete);

  ENTRY("Remove All Virtuals Cursors", "remove_all_multicursor", removeAllCursors());
	action->setShortcut(Qt::ALT + Qt::SHIFT + Qt::Key_Delete);

  ENTRY("Remove Virtuals Cursors Line", "remove_line_multicursor", removeCursorsOnLine());
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

  ENTRY("Remove All Virtuals Selections", "remove_all_multiselection", removeAllRanges());
  action->setShortcut(Qt::CTRL + Qt::Key_Underscore);

  ENTRY("Remove Virtuals Selections Line", "remove_line_multiselection", removeRangesOnline());

  ENTRY("Clear Virtuals Selections", "clear_multiselection", clearRanges());
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

void MultiCursorView::textBackspace()
{
  if (startEditing()) {
    CursorListDetail::removeBackwardText(
      m_document, m_cursors, 1,
      [](Cursor const &) { return true; }
    );
    endEditing();
  }
}

void MultiCursorView::textDelete()
{
  if (startEditing()) {
    CursorListDetail::removeForwardText(m_document, m_cursors, 1);
    endEditing();
  }
}

#define SIGNALMAN_OBJECT(O, F, P) F(O, SIGNAL(P), this, SLOT(P))
#define SIGNALMAN_DOC(F, P) SIGNALMAN_OBJECT(m_document, F, P)

#define SIGNALMAN_CHECK_VAR m_ranges
#define SIGNALMAN_CHECK(F) SIGNALMAN_CHECK_##F
#define SIGNALMAN_CHECK_connect SIGNALMAN_CHECK_VAR.empty()
#define SIGNALMAN_CHECK_disconnect !SIGNALMAN_CHECK_VAR.empty()

#define SIGNALMAN_CURSORS(F)\
  do { \
    SIGNALMAN_DOC(F, textRemoved(KTextEditor::Document*,KTextEditor::Range,QString));\
    SIGNALMAN_DOC(F, textInserted(KTextEditor::Document*,KTextEditor::Range));\
    if (SIGNALMAN_CHECK(F)) {\
      SIGNALMAN_DOC(F, exclusiveEditStart(KTextEditor::Document*));\
      SIGNALMAN_DOC(F, exclusiveEditEnd(KTextEditor::Document*));\
    }\
  } while (0)

void MultiCursorView::connectCursors()
{
  SIGNALMAN_CURSORS(connect);
}

void MultiCursorView::disconnectCursors()
{
  SIGNALMAN_CURSORS(disconnect);
  if (m_is_synchronized_cursor) {
    m_is_synchronized_cursor = false;
    disconnectSynchronizedCursors();
  }
}

#undef SIGNALMAN_CURSORS
#undef SIGNALMAN_CHECK_VAR

#define SIGNALMAN_CURSORS_SYNCHRONIZE(F)                              \
  KActionCollection* collec = m_view->actionCollection();             \
  do {                                                                \
    F(                                                                \
      collec->action("move_line_up"), SIGNAL(triggered(bool)),        \
      this, SLOT(moveCursorToUp()));                                  \
    F(                                                                \
      collec->action("move_line_down"), SIGNAL(triggered(bool)),      \
      this, SLOT(moveCursorToDown()));                                \
    F(                                                                \
      collec->action("move_cusor_left"), SIGNAL(triggered(bool)),     \
      this, SLOT(moveCursorToLeft()));                                \
    F(                                                                \
      collec->action("move_cursor_right"), SIGNAL(triggered(bool)),   \
      this, SLOT(moveCursorToRight()));                               \
    F(                                                                \
      collec->action("beginning_of_line"), SIGNAL(triggered(bool)),   \
      this, SLOT(moveCursorToBeginningOfLine()));                     \
    F(                                                                \
      collec->action("end_of_line"), SIGNAL(triggered(bool)),         \
      this, SLOT(moveCursorToEndOfLine()));                           \
    F(                                                                \
      collec->action("to_matching_bracket"), SIGNAL(triggered(bool)), \
      this, SLOT(moveCursorToMatchingBracket()));                     \
    F(                                                                \
      collec->action("word_left"), SIGNAL(triggered(bool)),           \
      this, SLOT(moveCursorToWordLeft()));                            \
    F(                                                                \
      collec->action("word_right"), SIGNAL(triggered(bool)),          \
      this, SLOT(moveCursorToWordRight()));                           \
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
}

#undef SIGNALMAN_RANGES
#undef SIGNALMAN_CHECK
#undef SIGNALMAN_CHECK_connect
#undef SIGNALMAN_CHECK_disconnect
#undef SIGNALMAN_CHECK_VAR
#undef SIGNALMAN_OBJECT
#undef SIGNALMAN_DOC

void MultiCursorView::connectSynchronizedRanges()
{
// TODO
  KActionCollection* collec = m_view->actionCollection();
  connect(
    collec->action("select_line_up"), SIGNAL(triggered(bool)),
    this, SLOT(selectLineUp()));
  connect(
    collec->action("select_line_down"), SIGNAL(triggered(bool)),
    this, SLOT(selectLineDown()));
  connect(
    collec->action("select_char_left"), SIGNAL(triggered(bool)),
    this, SLOT(selectCharLeft()));
  connect(
    collec->action("select_char_right"), SIGNAL(triggered(bool)),
    this, SLOT(selectCharRight()));
  connect(
    collec->action("select_word_left"), SIGNAL(triggered(bool)),
    this, SLOT(selectWordLeft()));
  connect(
    collec->action("select_word_right"), SIGNAL(triggered(bool)),
    this, SLOT(selectWordRight()));
  connect(
    collec->action("select_beginning_of_line"), SIGNAL(triggered(bool)),
    this, SLOT(selectBeginningOfLine()));
  connect(
    collec->action("select_end_of_line"), SIGNAL(triggered(bool)),
    this, SLOT(selectEndOfLine()));
  connect(
    collec->action("select_matching_bracket"), SIGNAL(triggered(bool)),
    this, SLOT(selectMatchingBracket()));
  connect(
    collec->action("select_page_up"), SIGNAL(triggered(bool)),
    this, SLOT(selectPageUp()));
  connect(
    collec->action("select_page_down"), SIGNAL(triggered(bool)),
    this, SLOT(selectPageDown()));
}

void MultiCursorView::disconnectSynchronizedRanges()
{
// TODO

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
  collec->action("remove_line_multicursor")->setEnabled(x);
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
  CursorListDetail::moveCursor(*this, first
  , [](Cursor const & cur) {
    const int l = cur.line();
    const int c = cur.column();
    return KTextEditor::Cursor(l-1, c);
  });
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
    first->setCursor(first->line() + 1, first->column());
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
  m_cursors.erase(
    std::unique(m_cursors.begin(), m_cursors.end()), m_cursors.end());
}

void MultiCursorView::moveCursorToEndOfLine()
{
  for (Cursor & c : m_cursors) {
    const int l = c.line();
    c.setCursor(KTextEditor::Cursor(l, m_document->lineLength(l)));
  }
  m_cursors.erase(
    std::unique(m_cursors.begin(), m_cursors.end()), m_cursors.end());
}

void MultiCursorView::moveCursorToWordLeft()
{
  CursorListDetail::moveCursorsByAction(
    "word_left", m_is_moved, m_view, m_cursors);
}

void MultiCursorView::moveCursorToWordRight()
{
  CursorListDetail::moveCursorsByAction(
    "word_right", m_is_moved, m_view, m_cursors);
}

void MultiCursorView::moveCursorToMatchingBracket()
{
  CursorListDetail::moveCursorsByAction(
    "to_matching_bracket", m_is_moved, m_view, m_cursors, false);
}

void MultiCursorView::setCursor(const KTextEditor::Cursor& cursor)
{
  auto it = lowerBound(m_cursors, m_view->cursorPosition());
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

void MultiCursorView::cursorsToRanges()
{
// TODO

}

void MultiCursorView::selectLineUp()
{
  RangeList ranges(std::move(m_ranges));
  m_ranges.reserve(ranges.size());
  if (m_view->selection()
   && m_view->selectionRange().start() == m_view->cursorPosition()) {
    for (Range & r : ranges) {
      auto const & cstart = r.start();
      const int line = std::max(cstart.line() - 1, 0);
      KTextEditor::Cursor c(line, cstart.column());
      setRange(KTextEditor::Range(c, r.end()), false);
    }
  }
  else {
    for (Range & r : ranges) {
      auto const & cend = r.end();
      const int cline = cend.line();
      const int line = std::max(cline - 1, 0);
      KTextEditor::Cursor c(line, cend.column());
      setRange(KTextEditor::Range(r.start(), c), false);
    }
  }
}

void MultiCursorView::selectLineDown()
{
  RangeList ranges(std::move(m_ranges));
  m_ranges.reserve(ranges.size());
  if (m_view->selection()
   && m_view->selectionRange().end() == m_view->cursorPosition()) {
    for (Range & r : ranges) {
      auto const & cend = r.end();
      const int cline = cend.line();
      const int line = std::min(cline + 1, m_document->lineLength(cline));
      KTextEditor::Cursor c(line, cend.column());
      setRange(KTextEditor::Range(r.start(), c), false);
    }
  }
  else {
    for (Range & r : ranges) {
      auto const & cstart = r.start();
      const int cline = cstart.line();
      const int line = std::min(cline + 1, m_document->lineLength(cline));
      KTextEditor::Cursor c(line, cstart.column());
      setRange(KTextEditor::Range(c, r.end()), false);
    }
  }
}

void MultiCursorView::selectCharRight()
{
  RangeList ranges(std::move(m_ranges));
  m_ranges.reserve(ranges.size());
  const int linemax = m_document->lines();
  if (m_view->selection()
   && m_view->selectionRange().end() == m_view->cursorPosition()) {
    for (Range & r : ranges) {
      auto const & cend = r.end();
      const int cline = cend.line();
      const int ccolumn = cend.column();
      if (ccolumn + 1 < m_document->lineLength(cline)) {
        KTextEditor::Cursor c(cline, ccolumn + 1);
        setRange(KTextEditor::Range(r.start(), c), false);
      }
      else if (linemax != cline + 1) {
        KTextEditor::Cursor c(cline + 1, 0);
        setRange(KTextEditor::Range(r.start(), c), false);
      }
    }
  }
  else {
    for (Range & r : ranges) {
      auto const & cstart = r.start();
      const int cline = cstart.line();
      const int ccolumn = cstart.column();
      if (ccolumn + 1 != m_document->lineLength(cline)) {
        KTextEditor::Cursor c(cline, ccolumn + 1);
        setRange(KTextEditor::Range(c, r.end()), false);
      }
      else if (linemax != cline + 1) {
        KTextEditor::Cursor c(cline + 1, 0);
        setRange(KTextEditor::Range(c, r.end()), false);
      }
    }
  }
}

void MultiCursorView::selectCharLeft()
{
  RangeList ranges(std::move(m_ranges));
  m_ranges.reserve(ranges.size());
  if (m_view->selection()
   && m_view->selectionRange().start() == m_view->cursorPosition()) {
    for (Range & r : ranges) {
      auto const & cstart = r.start();
      const int cline = cstart.line();
      const int ccolumn = cstart.column();
      if (ccolumn != 0) {
        KTextEditor::Cursor c(cline, ccolumn - 1);
        setRange(KTextEditor::Range(c, r.end()), false);
      }
      else if (cline != 0) {
        KTextEditor::Cursor c(cline - 1, m_document->lineLength(cline));
        setRange(KTextEditor::Range(c, r.end()), false);
      }
    }
  }
  else {
    for (Range & r : ranges) {
      auto const & cstart = r.end();
      const int cline = cstart.line();
      const int ccolumn = cstart.column();
      if (ccolumn != 0) {
        KTextEditor::Cursor c(cline, ccolumn - 1);
        setRange(KTextEditor::Range(r.start(), c), false);
      }
      else if (cline) {
        KTextEditor::Cursor c(cline - 1, m_document->lineLength(cline));
        setRange(KTextEditor::Range(r.start(), c), false);
      }
    }
  }
}

void MultiCursorView::selectPageUp()
{
// TODO

}

void MultiCursorView::selectPageDown()
{
// TODO

}


void MultiCursorView::selectBeginningOfLine()
{
// TODO

}

void MultiCursorView::selectEndOfLine()
{
// TODO

}

void MultiCursorView::selectWordRight()
{
// TODO

}

void MultiCursorView::selectWordLeft()
{
// TODO

}

void MultiCursorView::selectMatchingBracket()
{
// TODO

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
        std::min(it_start->start().toCursor(), range.start()),
        std::max((it_end-1)->end().toCursor(), range.end())
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
			setCursor(KTextEditor::Cursor(line, std::min(range.start().column(), m_document->lineLength(line))));
		}
	} else {
		setCursor(m_view->cursorPosition());
	}
}

void MultiCursorView::setActiveCursorCtrlClick(
  bool active, bool remove_cursor_if_only_click)
{
  m_remove_cursor_if_only_click = remove_cursor_if_only_click;
  setEventFilter(active);
  m_has_cursor_ctrl = active;
}

void MultiCursorView::setActiveSelectionCtrlClick(bool active)
{
  setEventFilter(active);
  m_has_selection_ctrl = active;
}

void MultiCursorView::setEventFilter(bool x)
{
  if ((m_has_selection_ctrl || m_has_cursor_ctrl) == x) {
    return;
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
  if (event->type() == QEvent::MouseButtonRelease) {
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
      first->revalid();
    }
    auto last = m_cursors.end();
    if (it != last) {
      if (m_view->cursorPosition() != it->cursor()) {
        m_document->insertText(it->cursor(), text);
        it->revalid();
      }
      while (++it != last) {
        m_document->insertText(it->cursor(), text);
        it->revalid();
      }
    }
		endEditing();
	}
}

void MultiCursorView::textRemoved(
  KTextEditor::Document* doc, const KTextEditor::Range& range,
  const QString& text)
{
  Q_UNUSED(doc);
  Q_UNUSED(range);
  // TODO block selection
  if (startEditing()) {
    CursorListDetail::removeBackwardText(
      m_document, m_cursors, text.length(),
      [this](Cursor const & c) {
        return c.cursor() != m_view->cursorPosition();
      }
    );
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
  return moving_range;
}

KTextEditor::MovingRange* MultiCursorView::newMovingRange(
  const KTextEditor::Range& range) const
{
  KTextEditor::MovingRange * moving_range = m_smart->newMovingRange(range);
  moving_range->setAttribute(m_selection_attr);
  return moving_range;
}

#include "multicursorview.moc"
