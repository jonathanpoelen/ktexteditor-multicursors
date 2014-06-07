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

#ifndef MCURSORS_CONFIG_H
#define MCURSORS_CONFIG_H

#include <KCModule>

class KColorButton;
class KComboBox;

class MultiCursorConfig
: public KCModule
{
	Q_OBJECT

public:
	explicit MultiCursorConfig(QWidget *parent = 0, const QVariantList &args = QVariantList());
	virtual ~MultiCursorConfig();

	virtual void save();
	virtual void load();
	virtual void defaults();

private Q_SLOTS:
	void slotChanged();

private:
	KColorButton * m_cursor_color;
	KComboBox * m_underline_style;
	KColorButton * m_underline_color;
};

#endif // MCURSORS_CONFIG_H
