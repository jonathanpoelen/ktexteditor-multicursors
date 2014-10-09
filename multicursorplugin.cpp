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
#include "multicursorconfig.h"

// #include <KPluginFactory>
// #include <KPluginLoader>
#include <QApplication>
#include <KConfigGroup>
#include <KTextEditor/View>

MultiCursorPlugin *MultiCursorPlugin::plugin = 0;

K_PLUGIN_FACTORY_DEFINITION(MultiCursorPluginFactory,
  registerPlugin<MultiCursorPlugin>("ktexteditor_multicursor");
  registerPlugin<MultiCursorConfig>("ktexteditor_multicursor_config");
)
K_EXPORT_PLUGIN(MultiCursorPluginFactory("ktexteditor_multicursor", "ktexteditor_plugins"))

MultiCursorPlugin::MultiCursorPlugin(QObject *parent, const QVariantList &args)
: KTextEditor::Plugin(parent)
, m_views()
, m_attr(new KTextEditor::Attribute)
, m_remove_cursor_if_only_click(false)
, m_active_ctrl_click(false)
{
  Q_UNUSED(args);
  plugin = this;

  readConfig();
}

MultiCursorPlugin::~MultiCursorPlugin()
{
  plugin = 0;
}

void MultiCursorPlugin::addView(KTextEditor::View *view)
{
  MultiCursorView *nview = new MultiCursorView(view, m_attr);
  if (m_active_ctrl_click) {
    nview->setActiveCtrlClick(true, m_remove_cursor_if_only_click);
  }
  m_views.append(nview);
}

void MultiCursorPlugin::removeView(KTextEditor::View *view)
{
  for (int z = 0; z < m_views.size(); z++) {
    if (m_views.at(z)->parentClient() == view) {
      MultiCursorView *nview = m_views.at(z);
      m_views.removeAll(nview);
      delete nview;
    }
  }
  if (m_active_ctrl_click && m_views.empty()) {
    QApplication::instance()->removeEventFilter(this);
  }
}

void MultiCursorPlugin::readConfig()
{
  KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
  const DefaultValues values;
  m_attr->setBackground(cg.readEntry("cursor_brush", values.cursorColor));
  m_attr->setUnderlineColor(cg.readEntry("underline_color", values.underlineColor));
  int line_style = cg.readEntry("underline_style", values.underlineStyle);
  m_attr->setUnderlineStyle(QTextCharFormat::UnderlineStyle(line_style));
  m_remove_cursor_if_only_click = cg.readEntry("remove_cursor_if_only_click", false);
  m_active_ctrl_click = cg.readEntry("active_ctrl_click", true);
}

void MultiCursorPlugin::writeConfig()
{
  KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
  cg.writeEntry("cursor_brush", m_attr->background().color());
  cg.writeEntry("underline_color", m_attr->underlineColor());
  cg.writeEntry("underline_style", int(m_attr->underlineStyle()));
  cg.writeEntry("remove_cursor_if_only_click", m_remove_cursor_if_only_click);
  cg.writeEntry("active_ctrl_click", m_active_ctrl_click);
}

void MultiCursorPlugin::setActiveCtrlClick(bool active, bool remove_cursor_if_only_click)
{
  m_active_ctrl_click = active;
  m_remove_cursor_if_only_click = remove_cursor_if_only_click;
  for (MultiCursorView * v: m_views) {
    v->setActiveCtrlClick(active, remove_cursor_if_only_click);
  }
}
