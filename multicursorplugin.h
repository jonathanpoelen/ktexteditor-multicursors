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
    QColor cursorColor = QColor(0, 0, 0, 40);
    int underlineStyle = 1;
    QColor underlineColor = Qt::red;
    bool activeCtrlClick = true;
    bool removeCursorIfOnlyClick = false;
  };

public:
  explicit MultiCursorPlugin(QObject *parent = 0, const QVariantList &args = QVariantList());
  virtual ~MultiCursorPlugin();

  static MultiCursorPlugin* self()
  { return plugin; }

  void addView (KTextEditor::View *view);
  void removeView (KTextEditor::View *view);

  void readConfig();
  void writeConfig();

  virtual void readConfig (KConfig *)
  {};
  virtual void writeConfig (KConfig *)
  {};

  void setCursorBrush(const QBrush& brush)
  { m_attr->setBackground(brush); }
  void setUnderlineStyle(QTextCharFormat::UnderlineStyle style)
  { m_attr->setUnderlineStyle(style); }
  void setUnderlineColor(const QColor& color)
  { m_attr->setUnderlineColor(color); }
  void setActiveCtrlClick(bool active, bool remove_cursor_if_only_click);

  QBrush cursorBrush() const
  { return m_attr->background(); }
  QTextCharFormat::UnderlineStyle underlineStyle() const
  { return m_attr->underlineStyle(); }
  QColor underlineColor() const
  { return m_attr->underlineColor(); }
  bool activeCtrlClick() const
  { return m_active_ctrl_click; }
  bool activeRemovedCursorIfOnlyClick() const
  { return m_remove_cursor_if_only_click; }

private:
  static MultiCursorPlugin *plugin;
  QList<MultiCursorView*> m_views;
  KTextEditor::Attribute::Ptr m_attr;
  bool m_remove_cursor_if_only_click;
  bool m_active_ctrl_click;
};

K_PLUGIN_FACTORY_DECLARATION(MultiCursorPluginFactory)

#endif
