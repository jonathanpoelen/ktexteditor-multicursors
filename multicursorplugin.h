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

#ifndef MULTICURSOR_PLUGIN_H
#define MULTICURSOR_PLUGIN_H

#include <QColor>
#include <QTextFormat>

#include <KTextEditor/Plugin>
#include <KTextEditor/Attribute>

namespace KTextEditor
{
  class View;
}

class MultiCursorView;


class MultiCursorPlugin
: public KTextEditor::Plugin
{
public:
  struct DefaultValues {
    struct Cursor {
      QColor color = QColor(0, 0, 0, 0);
      QTextCharFormat::UnderlineStyle underline_style
        = QTextCharFormat::SingleUnderline;
      QColor underline_color = Qt::red;
      bool active_ctrl_click = true;
      bool remove_cursor_if_only_click = false;
    } cursor;
    struct Selection {
      QColor color = Qt::lightGray;
      QTextCharFormat::UnderlineStyle underline_style = QTextCharFormat::NoUnderline;
      QColor underline_color = Qt::red;
      bool active_ctrl_click = true;
    } selection;
  };

public:
  explicit MultiCursorPlugin(QObject *parent = 0, const QVariantList &args = QVariantList());
  virtual ~MultiCursorPlugin();

  static MultiCursorPlugin* self()
  { return plugin; }

  void addView(KTextEditor::View *view);
  void removeView(KTextEditor::View *view);

  void readConfig();
  void writeConfig();

  virtual void readConfig(KConfig *)
  {}
  virtual void writeConfig(KConfig *)
  {}

  void setCursorBrush(const QBrush& brush)
  { m_cursor_attr->setBackground(brush); }
  void setCursorUnderlineStyle(QTextCharFormat::UnderlineStyle style)
  { m_cursor_attr->setUnderlineStyle(style); }
  void setCursorUnderlineColor(const QColor& color)
  { m_cursor_attr->setUnderlineColor(color); }

  void setActiveCursorCtrlClick(bool active, bool remove_cursor_if_only_click);

  void setSelectionBrush(const QBrush& brush)
  { m_selection_attr->setBackground(brush); }
  void setSelectionUnderlineStyle(QTextCharFormat::UnderlineStyle style)
  { m_selection_attr->setUnderlineStyle(style); }
  void setSelectionUnderlineColor(const QColor& color)
  { m_selection_attr->setUnderlineColor(color); }

  void setActiveSelectionCtrlClick(bool active);


  QBrush cursorBrush() const
  { return m_cursor_attr->background(); }
  QTextCharFormat::UnderlineStyle cursorUnderlineStyle() const
  { return m_cursor_attr->underlineStyle(); }
  QColor cursorUnderlineColor() const
  { return m_cursor_attr->underlineColor(); }
  bool activeCursorCtrlClick() const
  { return m_active_cursor_ctrl_click; }
  bool activeRemovedCursorIfOnlyClick() const
  { return m_remove_cursor_if_only_click; }

  QBrush selectionBrush() const
  { return m_selection_attr->background(); }
  QTextCharFormat::UnderlineStyle selectionUnderlineStyle() const
  { return m_selection_attr->underlineStyle(); }
  QColor selectionUnderlineColor() const
  { return m_selection_attr->underlineColor(); }
  bool activeSelectionCtrlClick() const
  { return m_active_selection_ctrl_click; }

private:
  static MultiCursorPlugin *plugin;
  QList<MultiCursorView*> m_views;
  KTextEditor::Attribute::Ptr m_cursor_attr;
  KTextEditor::Attribute::Ptr m_selection_attr;
  bool m_active_cursor_ctrl_click;
  bool m_remove_cursor_if_only_click;
  bool m_active_selection_ctrl_click;
};

K_PLUGIN_FACTORY_DECLARATION(MultiCursorPluginFactory)

#endif
