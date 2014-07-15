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

#ifndef MULTICURSOR_H
#define MULTICURSOR_H

#include <vector>
#include <memory>

#include <QObject>
#include <QColor>
#include <QTextFormat>

#include <KXMLGUIClient>
#include <KTextEditor/Plugin>
#include <KTextEditor/Cursor>
#include <KTextEditor/Range>
#include <KTextEditor/MovingCursor>
#include <KTextEditor/MovingRange>

namespace KTextEditor
{
	class View;
	class Document;
	class MovingInterface;
}

class MultiCursorView;


class MultiCursorPlugin
: public KTextEditor::Plugin
{
public:
	struct DefaultValues {
		QColor cursorColor;
		int underlineStyle;
		QColor underlineColor;

		DefaultValues()
		: cursorColor(0,0,0,40)
		, underlineStyle(1)
		, underlineColor(Qt::red)
		{}
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
	void setActiveCtrlClick(bool);

	QBrush cursorBrush() const
	{ return m_attr->background(); }
	QTextCharFormat::UnderlineStyle underlineStyle() const
	{ return m_attr->underlineStyle(); }
	QColor underlineColor() const
	{ return m_attr->underlineColor(); }
	bool activeCtrlClick() const
	{ return m_active_ctrl_click; }

protected:
	bool eventFilter(QObject *obj, QEvent *ev);

private:
	static MultiCursorPlugin *plugin;
	QList<MultiCursorView*> m_views;
	KTextEditor::Attribute::Ptr m_attr;
	MultiCursorView* m_last_active_view;
	bool m_active_ctrl_click;
};


class MultiCursorView
: public QObject
, public KXMLGUIClient
{
	Q_OBJECT

	class CursorListDetail;
public:
	explicit MultiCursorView(KTextEditor::View *view, KTextEditor::Attribute::Ptr);
	~MultiCursorView();

private:
	class Cursor
	{
	public:
		Cursor(KTextEditor::MovingRange * range) noexcept
		: m_range(range)
		{}

		Cursor(Cursor && other) = default;
		Cursor& operator=(Cursor && other) = default;
		Cursor& operator=(Cursor const & other) = delete;

		KTextEditor::Attribute::Ptr attribute()
		{ return m_range->attribute(); }

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

		void revalid()
		{ setCursor(line(), column()); }

	private:
		std::unique_ptr<KTextEditor::MovingRange> m_range;
	};
	///TODO boost::flat_set ?
	typedef std::vector<Cursor> CursorList;

private slots:
	void exclusiveEditStart(KTextEditor::Document*);
	void exclusiveEditEnd(KTextEditor::Document*);

	void textInserted(KTextEditor::Document*, const KTextEditor::Range&);
	void textRemoved(KTextEditor::Document*, const KTextEditor::Range&, const QString&);
	void textBackspace();
	void textDelete();

	void setCursor();
	void removeAll();
	void removeLine();

	void moveNext();
	void movePrev();

	void setActive();

	void setSynchronize();
	void cursorPositionChanged(KTextEditor::View*, const KTextEditor::Cursor&);

	//void debug() const;

protected:
	bool endEditing();
	bool startEditing();
	void insertText(const QString &text);

	void removeRange(const CursorList::iterator& it);
	void connectCurses();
	void disconnectCurses();
	void actionEmptyCurses();
	void actionStartCurses();
	void setEnabled(bool);
	KTextEditor::Cursor advance(const KTextEditor::Cursor&, int length, int endline) const;
	KTextEditor::Cursor recoil(const KTextEditor::Cursor&, int length, int minline = 0) const;

private:
	void removeTextNext(int length);
	void removeTextPrev(int length);

	void setCursor(const KTextEditor::Cursor& cursor);

public:
	void setCursorPosition();
	bool isActiveView() const;

private:
	KTextEditor::View *m_view;
	KTextEditor::Document *m_document;
	KTextEditor::MovingInterface *m_smart;
	bool m_text_edit;
	bool m_active;
	bool m_synchronize;
	KTextEditor::Cursor m_cursor;
	CursorList m_cursors;
	KTextEditor::Attribute::Ptr m_attr;
};

K_PLUGIN_FACTORY_DECLARATION(MultiCursorPluginFactory)

#endif
