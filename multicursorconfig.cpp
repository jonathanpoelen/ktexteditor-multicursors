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

#include "multicursorconfig.h"
#include "multicursorplugin.h"

#include <KConfigGroup>
#include <KColorButton>
#include <KComboBox>
#include <KLineEdit>
#include <QtGui/QTextCharFormat>
#include <QtGui/QBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QGroupBox>

namespace {
  template<class Ws>
  void build_widget_decoration(
    MultiCursorConfig * this_, QVBoxLayout * layout, Ws & w) {
    QHBoxLayout * hlayout = new QHBoxLayout(this_);
    w.color = new KColorButton(this_);
    w.color->setAlphaChannelEnabled(true);
    hlayout->addWidget(new QLabel(i18n("Baground color")));
    hlayout->addWidget(w.color);
    layout->addLayout(hlayout);

    hlayout = new QHBoxLayout(this_);
    w.underline_style = new KComboBox(this_);
    w.underline_style->addItem(i18n("None"));
    w.underline_style->addItem(i18n("Single"));
    w.underline_style->addItem(i18n("Dash"));
    w.underline_style->addItem(i18n("Dot line"));
    w.underline_style->addItem(i18n("Dash dot line"));
    w.underline_style->addItem(i18n("Dash dot dot line"));
    hlayout->addWidget(new QLabel(i18n("Underline style")));
    hlayout->addWidget(w.underline_style);
    layout->addLayout(hlayout);

    hlayout = new QHBoxLayout(this_);
    w.underline_color = new KColorButton(this_);
    w.underline_color_label = new QLabel(i18n("Underline color"));
    hlayout->addWidget(w.underline_color_label);
    hlayout->addWidget(w.underline_color);
    layout->addLayout(hlayout);
  }
}

MultiCursorConfig::MultiCursorConfig(QWidget *parent, const QVariantList &args)
: KCModule(MultiCursorPluginFactory::componentData(), parent, args)
{
  QVBoxLayout * glayout = new QVBoxLayout(this);
  QGroupBox * group = new QGroupBox(this);
  group->setTitle(i18n("Virtual cursor"));
  QVBoxLayout * layout = new QVBoxLayout(group);
  group->setLayout(layout);
  glayout->addWidget(group);

  build_widget_decoration(this, layout, w.cursor);

  w.cursor.active_ctrl_click
    = new QCheckBox(i18n("Set cursor with Ctrl+Click"), this);
  layout->addWidget(w.cursor.active_ctrl_click);

  w.cursor.remove_cursor_if_only_click
    = new QCheckBox(i18n("Removing all cursors on click without Ctrl"), this);
  layout->addWidget(w.cursor.remove_cursor_if_only_click);

  group = new QGroupBox(this);
  group->setTitle(i18n("Virtual selection"));
  layout = new QVBoxLayout(group);
  group->setLayout(layout);
  glayout->addWidget(group);

  build_widget_decoration(this, layout, w.selection);

  w.selection.active_ctrl_click
    = new QCheckBox(i18n("Set selection with Ctrl+Click"), this);
  layout->addWidget(w.selection.active_ctrl_click);

  w.active_remove_all_if_esc = new QCheckBox(
    i18n("Remove all cursors and selections if Esc is pressed"), this);
  glayout->addWidget(w.active_remove_all_if_esc);

  setLayout(glayout);

  //load();

  QObject::connect(
    w.cursor.color, SIGNAL(changed(QColor)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.cursor.underline_color, SIGNAL(changed(QColor)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.cursor.underline_style, SIGNAL(currentIndexChanged(int)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.cursor.active_ctrl_click, SIGNAL(stateChanged(int)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.cursor.remove_cursor_if_only_click, SIGNAL(stateChanged(int)),
    this, SLOT(slotChanged()));

  QObject::connect(
    w.selection.color, SIGNAL(changed(QColor)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.selection.underline_color, SIGNAL(changed(QColor)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.selection.underline_style, SIGNAL(currentIndexChanged(int)),
    this, SLOT(slotChanged()));
  QObject::connect(
    w.selection.active_ctrl_click, SIGNAL(stateChanged(int)),
    this, SLOT(slotChanged()));

  QObject::connect(
    w.active_remove_all_if_esc, SIGNAL(stateChanged(int)),
    this, SLOT(slotChanged()));


  QObject::connect(
    w.cursor.underline_style, SIGNAL(currentIndexChanged(int)),
    this, SLOT(underlineStyleCursorChanged(int)));

  QObject::connect(
    w.selection.underline_style, SIGNAL(currentIndexChanged(int)),
    this, SLOT(underlineStyleSelectionChanged(int)));

  QObject::connect(
    w.cursor.active_ctrl_click, SIGNAL(toggled(bool)),
    w.cursor.remove_cursor_if_only_click, SLOT(setEnabled(bool)));
}

MultiCursorConfig::~MultiCursorConfig()
{
}

void MultiCursorConfig::underlineStyleCursorChanged(int index)
{
  w.cursor.underline_color->setEnabled(index);
  w.cursor.underline_color_label->setEnabled(index);
}

void MultiCursorConfig::underlineStyleSelectionChanged(int index)
{
  w.selection.underline_color->setEnabled(index);
  w.selection.underline_color_label->setEnabled(index);
}

void MultiCursorConfig::save()
{
  if (MultiCursorPlugin::self())
  {
    MultiCursorPlugin * self = MultiCursorPlugin::self();

    self->setCursorBrush(w.cursor.color->color());
    self->setCursorUnderlineStyle(QTextCharFormat::UnderlineStyle(
      w.cursor.underline_style->currentIndex()));
    self->setCursorUnderlineColor(w.cursor.underline_color->color());

    self->setActiveCursorCtrlClick(
      w.cursor.active_ctrl_click->isChecked(),
      w.cursor.remove_cursor_if_only_click->isChecked());

    self->setSelectionBrush(w.selection.color->color());
    self->setSelectionUnderlineStyle(QTextCharFormat::UnderlineStyle(
      w.selection.underline_style->currentIndex()));
    self->setSelectionUnderlineColor(w.selection.underline_color->color());

    self->setActiveSelectionCtrlClick(
      w.selection.active_ctrl_click->isChecked());

    self->setActiveRemoveAllIfEsc(w.active_remove_all_if_esc->isChecked());

    self->writeConfig();
  }
  else
  {
    KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
    cg.writeEntry("cursor_color", w.cursor.color->color());
    cg.writeEntry("underline_style", w.cursor.underline_style->currentIndex());
    cg.writeEntry("underline_color", w.cursor.underline_color->text());

    cg.writeEntry(
      "active_ctrl_click",
      w.cursor.active_ctrl_click->isChecked());
    cg.writeEntry(
      "remove_cursor_if_only_click",
      w.cursor.remove_cursor_if_only_click->isChecked());

    cg.writeEntry("bg_selection", w.selection.color->color());
    cg.writeEntry(
      "underline_style_selection" ,
      w.selection.underline_style->currentIndex());
    cg.writeEntry(
      "underline_color_selection",
      w.selection.underline_color->text());

    cg.writeEntry(
      "active_ctrl_click_selection",
      w.selection.active_ctrl_click->isChecked());

    cg.writeEntry(
      "active_remove_all_if_esc",
      w.active_remove_all_if_esc->isChecked());
  }
  emit changed(false);
}

void MultiCursorConfig::load()
{
  if (MultiCursorPlugin::self())
  {
    MultiCursorPlugin * self = MultiCursorPlugin::self();
    self->readConfig();
    w.cursor.color->setColor(self->cursorBrush().color());
    w.cursor.underline_color->setColor(self->cursorUnderlineColor());
    w.cursor.underline_style->setCurrentIndex(self->cursorUnderlineStyle());

    w.cursor.active_ctrl_click->setChecked(self->activeCursorCtrlClick());
    w.cursor.remove_cursor_if_only_click->setChecked(
      self->activeRemovedCursorIfOnlyClick());

    w.selection.color->setColor(self->selectionBrush().color());
    w.selection.underline_color->setColor(self->selectionUnderlineColor());
    w.selection.underline_style->setCurrentIndex(
      self->selectionUnderlineStyle());

    w.selection.active_ctrl_click->setChecked(self->activeSelectionCtrlClick());

    w.active_remove_all_if_esc->setChecked(self->activeRemoveAllIfEsc());
  }
  else
  {
    KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
    const MultiCursorPlugin::DefaultValues values;
    w.cursor.color->setColor(
      cg.readEntry("cursor_color", values.cursor.color));
    w.cursor.underline_color->setColor(
      cg.readEntry("underline_color", values.cursor.underline_color));
    w.cursor.underline_style->setCurrentIndex(
      cg.readEntry("underline_style", int(values.cursor.underline_style)));

    w.cursor.remove_cursor_if_only_click->setChecked(
      cg.readEntry(
        "remove_cursor_if_only_click",
        values.cursor.remove_cursor_if_only_click));
    w.cursor.active_ctrl_click->setChecked(
      cg.readEntry("active_ctrl_click", values.cursor.active_ctrl_click));

    w.selection.color->setColor(
      cg.readEntry("bg_selection", values.selection.color));
    w.selection.underline_color->setColor(
      cg.readEntry(
        "underline_color_selection",
        values.selection.underline_color));
    w.selection.underline_style->setCurrentIndex(
      cg.readEntry(
        "underline_style_selection",
        int(values.selection.underline_style)));

    w.selection.active_ctrl_click->setChecked(
      cg.readEntry(
        "active_ctrl_click_selection", values.selection.active_ctrl_click));

    w.active_remove_all_if_esc->setChecked(
      cg.readEntry(
        "active_remove_all_if_esc", values.m_active_remove_all_if_esc));
  }

  if (!w.cursor.active_ctrl_click->isChecked()) {
    w.cursor.remove_cursor_if_only_click->setEnabled(false);
  }
  if (w.cursor.underline_style->currentIndex() == 0) {
    w.cursor.underline_color->setEnabled(false);
    w.cursor.underline_color_label->setEnabled(false);
  }
  if (w.selection.underline_style->currentIndex() == 0) {
    w.selection.underline_color->setEnabled(false);
    w.selection.underline_color_label->setEnabled(false);
  }

  emit changed(false);
}

void MultiCursorConfig::defaults()
{
  const MultiCursorPlugin::DefaultValues values;

  w.cursor.color->setColor(values.cursor.color);
  w.cursor.underline_style->setCurrentIndex(values.cursor.underline_style);
  w.cursor.underline_color->setColor(values.cursor.underline_color);

  w.cursor.active_ctrl_click->setChecked(values.cursor.active_ctrl_click);
  w.cursor.remove_cursor_if_only_click->setChecked(
    values.cursor.remove_cursor_if_only_click);

  w.selection.color->setColor(values.selection.color);
  w.selection.underline_style->setCurrentIndex(
    values.selection.underline_style);
  w.selection.underline_color->setColor(values.selection.underline_color);

  w.selection.active_ctrl_click->setChecked(
    values.selection.active_ctrl_click);

  w.active_remove_all_if_esc->setChecked(values.m_active_remove_all_if_esc);

  emit changed(true);
}

void MultiCursorConfig::slotChanged()
{
  emit changed(true);
}

#include "multicursorconfig.moc"
