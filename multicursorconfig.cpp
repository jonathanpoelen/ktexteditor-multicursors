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
#include "multicursor.h"

#include <KLocale>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KConfigGroup>
#include <KColorButton>
#include <KComboBox>
#include <KLineEdit>
#include <QtGui/QTextCharFormat>
#include <QtGui/QBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QLabel>
#include <QtGui/QFrame>


MultiCursorConfig::MultiCursorConfig(QWidget *parent, const QVariantList &args)
: KCModule(MultiCursorPluginFactory::componentData(), parent, args)
{
	QVBoxLayout * layout = new QVBoxLayout(this);

	QHBoxLayout * hlayout = new QHBoxLayout(this);
	m_cursor_color = new KColorButton(this);
	m_cursor_color->setAlphaChannelEnabled(true);
	hlayout->addWidget(new QLabel(i18n("Background color cursor")));
	hlayout->addWidget(m_cursor_color);
	layout->addLayout(hlayout);

	hlayout = new QHBoxLayout(this);
	m_underline_style = new KComboBox(this);
	m_underline_style->addItem(i18n("None"));
	m_underline_style->addItem(i18n("Single"));
	m_underline_style->addItem(i18n("Dash"));
	m_underline_style->addItem(i18n("Dot line"));
	m_underline_style->addItem(i18n("Dash dot line"));
	m_underline_style->addItem(i18n("Dash dot dot line"));
	hlayout->addWidget(new QLabel(i18n("Underline style")));
	hlayout->addWidget(m_underline_style);
	layout->addLayout(hlayout);

	hlayout = new QHBoxLayout(this);
	m_underline_color = new KColorButton(this);
	QLabel * lunderline_color = new QLabel(i18n("Underline color"));
	hlayout->addWidget(lunderline_color);
	hlayout->addWidget(m_underline_color);
	layout->addLayout(hlayout);

	hlayout = new QHBoxLayout(this);
	m_active_ctrl_click = new QCheckBox(i18n("Set cursor with Ctrl+Click"), this);
	layout->addWidget(m_active_ctrl_click);

	hlayout = new QHBoxLayout(this);
	m_remove_cursor_if_only_click = new QCheckBox(i18n("Removing all cursors on click without Ctrl"), this);
	layout->addWidget(m_remove_cursor_if_only_click);

	setLayout(layout);

	load();

	QObject::connect(m_cursor_color, SIGNAL(changed(QColor)),
									 this, SLOT(slotChanged()));
	QObject::connect(m_underline_color, SIGNAL(changed(QColor)),
									 this, SLOT(slotChanged()));
	QObject::connect(m_underline_style, SIGNAL(currentIndexChanged(int)),
									 this, SLOT(slotChanged()));
	QObject::connect(m_active_ctrl_click, SIGNAL(stateChanged(int)),
									 this, SLOT(slotChanged()));
	QObject::connect(m_remove_cursor_if_only_click, SIGNAL(stateChanged(int)),
									 this, SLOT(slotChanged()));

	QObject::connect(m_underline_style, SIGNAL(currentIndexChanged(int)),
									 m_underline_color, SLOT(setEnabled(bool)));
	QObject::connect(m_underline_style, SIGNAL(currentIndexChanged(int)),
									 lunderline_color, SLOT(setEnabled(bool)));
	QObject::connect(m_active_ctrl_click, SIGNAL(toggled(bool)),
									 m_remove_cursor_if_only_click, SLOT(setEnabled(bool)));
}

MultiCursorConfig::~MultiCursorConfig()
{
}

// void MultiCursorConfig::underlineStyleindexChanged(int index)
// {
// 	underline_frame.setEnabled(index);
// }

void MultiCursorConfig::save()
{
	if (MultiCursorPlugin::self())
	{
		MultiCursorPlugin * self = MultiCursorPlugin::self();
		self->setCursorBrush(m_cursor_color->color());
		int index = m_underline_style->currentIndex();
		self->setUnderlineStyle(QTextCharFormat::UnderlineStyle(index));
		self->setUnderlineColor(m_underline_color->color());
		self->setActiveCtrlClick(m_active_ctrl_click->isChecked(), m_remove_cursor_if_only_click->isChecked());
		self->writeConfig();
	}
	else
	{
		KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
		cg.writeEntry("cursor_color", m_cursor_color->color());
		cg.writeEntry("underline_style", m_underline_style->currentIndex());
		cg.writeEntry("underline_color", m_underline_color->text());
		cg.writeEntry("active_ctrl_click", m_active_ctrl_click->isChecked());
		cg.writeEntry("remove_cursor_if_only_click", m_remove_cursor_if_only_click->isChecked());
	}
	emit changed(false);
}

void MultiCursorConfig::load()
{
	if (MultiCursorPlugin::self())
	{
		MultiCursorPlugin * self = MultiCursorPlugin::self();
		self->readConfig();
		m_cursor_color->setColor(self->cursorBrush().color());
		m_underline_color->setColor(self->underlineColor());
		m_underline_style->setCurrentIndex(self->underlineStyle());
		m_active_ctrl_click->setChecked(self->activeCtrlClick());
		m_remove_cursor_if_only_click->setChecked(self->activeRemovedCursorIfOnlyClick());
	}
	else
	{
		KConfigGroup cg(KGlobal::config(), "MultiCursor Plugin");
		const MultiCursorPlugin::DefaultValues values;
		m_cursor_color->setColor(cg.readEntry("cursor_color", values.cursorColor));
		m_underline_color->setColor(cg.readEntry("underline_color", values.underlineColor));
		m_underline_style->setCurrentIndex(cg.readEntry("underline_style", values.underlineStyle));
		m_remove_cursor_if_only_click->setChecked(cg.readEntry("remove_cursor_if_only_click" , values.activeCtrlClick));
		m_active_ctrl_click->setChecked(cg.readEntry("active_ctrl_click" , values.activeCtrlClick));
	}
	emit changed(false);
}

void MultiCursorConfig::defaults()
{
	const MultiCursorPlugin::DefaultValues values;
	m_cursor_color->setColor(values.cursorColor);
	m_underline_style->setCurrentIndex(values.underlineStyle);
	m_underline_color->setColor(values.underlineColor);
	m_active_ctrl_click->setCheckable(true);
	m_active_ctrl_click->setChecked(true);
	emit changed(true);
}

void MultiCursorConfig::slotChanged()
{
	emit changed(true);
}

#include "multicursorconfig.moc"
