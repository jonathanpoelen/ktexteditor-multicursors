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

#include <KConfigGroup>
#include <KTextEditor/View>

MultiCursorPlugin *MultiCursorPlugin::plugin = 0;

K_PLUGIN_FACTORY_DEFINITION(MultiCursorPluginFactory,
  registerPlugin<MultiCursorPlugin>("ktexteditor_multicursor");
  registerPlugin<MultiCursorConfig>("ktexteditor_multicursor_config");
)
K_EXPORT_PLUGIN(MultiCursorPluginFactory(
  "ktexteditor_multicursor", "ktexteditor_plugins")
)

MultiCursorPlugin::MultiCursorPlugin(QObject *parent, const QVariantList &)
: KTextEditor::Plugin(parent)
, m_cursor_attr(new KTextEditor::Attribute)
, m_selection_attr(new KTextEditor::Attribute)
, m_active_cursor_ctrl_click(false)
, m_remove_cursor_if_only_click(false)
, m_active_selection_ctrl_click(false)
, m_active_remove_all_if_esc(false)
{
  plugin = this;

  readConfig();
}

MultiCursorPlugin::~MultiCursorPlugin()
{
  plugin = nullptr;
}

void MultiCursorPlugin::addView(KTextEditor::View *view)
{
  MultiCursorView *nview = new MultiCursorView(
    view, m_cursor_attr, m_selection_attr);
  if (m_active_cursor_ctrl_click) {
    nview->setActiveCursorCtrlClick(true, m_remove_cursor_if_only_click);
  }
  if (m_active_selection_ctrl_click) {
    nview->setActiveSelectionCtrlClick(true);
  }
  if (m_active_remove_all_if_esc) {
    nview->setActiveRemoveAllIfEsc(true);
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
}

void MultiCursorPlugin::readConfig()
{
  KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
  const DefaultValues values;
  m_cursor_attr->setBackground(
    cg.readEntry("cursor_color", values.cursor.color));
  m_cursor_attr->setUnderlineColor(
    cg.readEntry("underline_color", values.cursor.underline_color));
  m_cursor_attr->setUnderlineStyle(QTextCharFormat::UnderlineStyle(
    cg.readEntry("underline_style", int(values.cursor.underline_style))));
  m_remove_cursor_if_only_click
    = cg.readEntry("remove_cursor_if_only_click", false);
  m_active_cursor_ctrl_click = cg.readEntry("active_ctrl_click", true);

  m_selection_attr->setBackground(
    cg.readEntry("bg_selection", values.selection.color));
  m_selection_attr->setUnderlineColor(
    cg.readEntry(
      "underline_color_selection", values.selection.underline_color));
  m_selection_attr->setUnderlineStyle(QTextCharFormat::UnderlineStyle(
    cg.readEntry(
      "underline_style_selection", int(values.selection.underline_style))));
  m_active_selection_ctrl_click = cg.readEntry("active_ctrl_click_selection", true);

  m_active_remove_all_if_esc = cg.readEntry("active_remove_all_if_esc", false);
}

void MultiCursorPlugin::writeConfig()
{
  KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
  cg.writeEntry("cursor_color", m_cursor_attr->background().color());
  cg.writeEntry("underline_color", m_cursor_attr->underlineColor());
  cg.writeEntry("underline_style", int(m_cursor_attr->underlineStyle()));
  cg.writeEntry("remove_cursor_if_only_click", m_remove_cursor_if_only_click);
  cg.writeEntry("active_ctrl_click", m_active_cursor_ctrl_click);

  cg.writeEntry("bg_selection", m_selection_attr->background().color());
  cg.writeEntry(
    "underline_color_selection", m_selection_attr->underlineColor());
  cg.writeEntry(
    "underline_style_selection", int(m_selection_attr->underlineStyle()));
  cg.writeEntry("active_ctrl_click_selection", m_active_selection_ctrl_click);

  cg.writeEntry("active_remove_all_if_esc", m_active_remove_all_if_esc);
}

void MultiCursorPlugin::setActiveCursorCtrlClick(
  bool active, bool remove_cursor_if_only_click
) {
  m_active_cursor_ctrl_click = active;
  m_remove_cursor_if_only_click = remove_cursor_if_only_click;
  for (MultiCursorView * v: m_views) {
    v->setActiveCursorCtrlClick(active, remove_cursor_if_only_click);
  }
}

void MultiCursorPlugin::setActiveSelectionCtrlClick(bool active)
{
  m_active_selection_ctrl_click = active;
  for (MultiCursorView * v: m_views) {
    v->setActiveSelectionCtrlClick(active);
  }
}

void MultiCursorPlugin::setActiveRemoveAllIfEsc(bool active)
{
  m_active_remove_all_if_esc = active;
  for (MultiCursorView * v: m_views) {
    v->setActiveRemoveAllIfEsc(active);
  }
}

