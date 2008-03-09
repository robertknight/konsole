/*
    Copyright 2007-8 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef SCREENWINDOW_H
#define SCREENWINDOW_H

// Qt
#include <QtCore/QBitArray>
#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QRect>

// Konsole
#include "Character.h"
#include "TerminalCharacterDecoder.h"

namespace Konsole
{

class Screen;

/** 
 * Support class for ScreenWindow.
 * Stores fold points associated with a buffer containing lines of text and provides
 * methods to query the visibility of individual lines depending on the state 
 * (expanded of collapsed) of those fold points.
 *
 * The line visiblity information must be updated manually after any changes to folds
 * or the line count by calling updateVisibleLines()
 */
class Folds
{
public:
	/** 
	 * Create a fold between startLine and endLine, which is initially expanded if @p open is true
	 * or collapsed otherwise.
	 */
	void setFold(int startLine,int endLine, bool open);
	/** Removes all fold points */
	void removeAll();
	enum Type
	{
		/** There are no fold points on the line */
		FoldNone,
		/** The line is the start of a fold */
		FoldStart,
		/** The line is the end of a fold */
		FoldEnd
	};
	/** Returns the type of fold point at a particular line */
	Type type(int line) const;
	/** Returns true if the fold point at @p line is expanded or false if it is collapsed. */
	bool isOpen(int line) const;
	/** Sets whether the fold point at @p line is expanded or collapsed. */
	void setOpen(int line,bool open);
	/** Returns the total number of fold points. */
	int count() const;
	/** Sets the number of lines in the associated buffer. */
	void setLineCount(int count);

	/** Updates the visibility information for each line in the buffer. */
	void updateVisibleLines();
	/** 
	 * Returns true if @p line is currently visible.  This information is correct 
	 * at the time updateVisibleLines() was last called.
	 */
	bool isLineVisible(int line) const;
	/** 
	 * Returns the total number of visible lines, correct at the last time
	 * updateVisibleLines() was called.
	 */
	int visibleLineCount() const;
	/**
	 * Returns the original line index of a visible line.
	 *
	 * If there are no hidden lines before @p visibleLine then the result
	 * will be @p visibleLine.  Otherwise the result will be @p visibleLine plus
	 * the number of hidden lines before it.
	 *
	 * @p visibleLine may be greater than the line count set with setLineCount(), in which
	 * case the result will be the line count set with setLineCount()
	 *
	 * Performance is O(n), where n is @p visibleLine
	 */
	int mapToBufferLine(int visibleLine) const;

	/**
	 * Maps a line index in the buffer to the visible line.  The result will be 
	 * @p bufferLine minus the number of hidden lines before it.  If @p bufferLine 
	 * is not visible and @p assumeVisible is false then the result will be -1
	 *
	 * If @p assumeVisible is true then @p bufferLine is treated as if it was visible,
	 * even if it is actually hidden, in which case the result will be the count
	 * of visible lines before @p bufferLine
	 *
	 * @p bufferLine may be greater than the line count set with setLineCount(), in which
	 * case the result will be visibleLineCount()
	 *
	 * Performance is O(n), where n is @p bufferLine
	 */
	int mapToVisibleLine(int bufferLine , bool assumeVisible) const;

private:

	bool _enabled;
	QBitArray _foldStarts;
	QBitArray _foldEnds;
	QBitArray _expanded;
	QBitArray _filteredLines;
	int _visibleLines;
};

inline bool Folds::isLineVisible(int line) const
{
	return _filteredLines.testBit(line);
}
inline int Folds::visibleLineCount() const
{
	return _enabled ? _visibleLines : _filteredLines.count();
}
inline int Folds::count() const
{
	return _enabled ? _foldStarts.count(true) : 0;
}
inline int Folds::mapToBufferLine(int visibleLine) const
{
	if (!_enabled)
		return visibleLine;
	else
	{
		int result = 0;
		int visibleCount = 0;
		while (visibleCount < visibleLine && result < _filteredLines.count()-1)
		{
			visibleCount += isLineVisible(result);
			result++;
		}
		return result;
	}		
}

/**
 * Provides a window onto a section of a terminal screen.
 * This window can then be rendered by a terminal display widget ( TerminalDisplay ).
 *
 * To use the screen window, create a new ScreenWindow() instance and associated it with 
 * a terminal screen using setScreen().
 * Use the scrollTo() method to scroll the window up and down on the screen.
 * Call the getImage() method to retrieve the character image which is currently visible in the window.
 *
 * setTrackOutput() controls whether the window moves to the bottom of the associated screen when new
 * lines are added to it.
 *
 * Whenever the output from the underlying screen is changed, the notifyOutputChanged() slot should
 * be called.  This in turn will update the window's position and emit the outputChanged() signal
 * if necessary.
 */
class ScreenWindow : public QObject
{
Q_OBJECT

public:
    /** 
     * Constructs a new screen window with the given parent.
     * A screen must be specified by calling setScreen() before calling getImage() or getLineProperties().
     *
     * You should not call this constructor directly, instead use the Emulation::createWindow() method
     * to create a window on the emulation which you wish to view.  This allows the emulation
     * to notify the window when the associated screen has changed and synchronize selection updates
     * between all views on a session.
     */
    ScreenWindow(QObject* parent = 0);
	virtual ~ScreenWindow();

    /** Sets the screen which this window looks onto */
    void setScreen(Screen* screen);
    /** Returns the screen which this window looks onto */
    Screen* screen() const;

    /** 
     * Returns the image of characters which are currently visible through this window
     * onto the screen.
     *
     * The buffer is managed by the ScreenWindow instance and does not need to be
     * deleted by the caller.
     */
    Character* getImage();

    /**
     * Returns the line attributes associated with the lines of characters which
     * are currently visible through this window
     */
    QVector<LineProperty> getLineProperties();

    /**
     * Returns the number of lines which the region of the window
     * specified by scrollRegion() has been scrolled by since the last call 
     * to resetScrollCount().  scrollRegion() is in most cases the 
     * whole window, but will be a smaller area in, for example, applications
     * which provide split-screen facilities.
     *
     * This is not guaranteed to be accurate, but allows views to optimise
     * rendering by reducing the amount of costly text rendering that
     * needs to be done when the output is scrolled. 
     */
    int scrollCount() const;

    /**
     * Resets the count of scrolled lines returned by scrollCount()
     */
    void resetScrollCount();

    /**
     * Returns the area of the window which was last scrolled, this is 
     * usually the whole window area.
     *
     * Like scrollCount(), this is not guaranteed to be accurate,
     * but allows views to optimise rendering.
     */
    QRect scrollRegion() const;

    /** 
     * Sets the start of the selection to the given @p line and @p column within 
     * the window.
     */
    void setSelectionStart( int column , int line , bool columnMode );
    /**
     * Sets the end of the selection to the given @p line and @p column within
     * the window.
     */
    void setSelectionEnd( int column , int line ); 
    /**
     * Retrieves the start of the selection within the window.
     */
    void getSelectionStart( int& column , int& line );
    /**
     * Retrieves the end of the selection within the window.
     */
    void getSelectionEnd( int& column , int& line );
    /**
     * Returns true if the character at @p line , @p column is part of the selection.
     */
    bool isSelected( int column , int line );
    /** 
     * Clears the current selection
     */
    void clearSelection();

	/** Sets the number of lines in the window */
	void setWindowLines(int lines);
    /** Returns the number of lines in the window */
    int windowLines() const;
    /** Returns the number of columns in the window */
    int windowColumns() const;
    
    /** Returns the total number of lines in the screen */
    int lineCount() const;
	/** 
	 * Returns the number of visible lines in the screen in this window.
	 * This is lineCount() minus the number of lines hidden because they are
	 * inside closed folds.
	 *
	 * If there are no closed folds, this will return lineCount()
	 */
	int visibleLineCount() const;

    /** Returns the total number of columns in the screen */
    int columnCount() const;

    /** Returns the index of the line which is currently at the top of this window */
    int currentLine() const;

    /** 
     * Returns the position of the cursor 
     * within the window.
     */
    QPoint cursorPosition() const;

    /** 
     * Convenience method. Returns true if the window is currently at the bottom
     * of the screen.
     */
    bool atEndOfOutput() const;

    /** Scrolls the window so that @p line is at the top of the window */
    void scrollTo( int line );

    enum RelativeScrollMode
    {
        ScrollLines,
        ScrollPages
    };

    /** 
     * Scrolls the window relative to its current position on the screen.
     *
     * @param mode Specifies whether @p amount refers to the number of lines or the number
     * of pages to scroll.    
     * @param amount The number of lines or pages ( depending on @p mode ) to scroll by.  If
     * this number is positive, the view is scrolled down.  If this number is negative, the view
     * is scrolled up.
     */
    void scrollBy( RelativeScrollMode mode , int amount );

    /** 
     * Specifies whether the window should automatically move to the bottom
     * of the screen when new output is added.
     *
     * If this is set to true, the window will be moved to the bottom of the associated screen ( see 
     * screen() ) when the notifyOutputChanged() method is called.
     */
    void setTrackOutput(bool trackOutput);
    /** 
     * Returns whether the window automatically moves to the bottom of the screen as
     * new output is added.  See setTrackOutput()
     */
    bool trackOutput() const;

    /**
     * Returns the text which is currently selected.
     *
     * @param preserveLineBreaks See Screen::selectedText()
     */
    QString selectedText( bool preserveLineBreaks ) const;

	// TODO: The fold commands take line arguments in 'Screen' coordinates
	// whereas the other commands in ScreenWindow take line arguments in
	// window coordinates and convert to 'Screen' coordinates
	
public slots:
    /** 
     * Notifies the window that the contents of the associated terminal screen have changed.
     * This moves the window to the bottom of the screen if trackOutput() is true and causes
     * the outputChanged() signal to be emitted.
     */
    void notifyOutputChanged();

	/** 
	 * Filter the contents of this window based on a pattern.
	 * Lines whoose text matches @p pattern will remain visible, other lines will
	 * be hidden.  If @p pattern is empty the filter will be removed and all lines 
	 * will be shown.
	 *
	 * In addition to lines matching the filter, the line containing the cursor will
	 * always be shown.
	 */
	void setFilter(const QRegExp& pattern);

signals:
    /**
     * Emitted when the contents of the associated terminal screen ( see screen() ) changes. 
     */
    void outputChanged();

    /**
     * Emitted when the screen window is scrolled to a different position.
     * 
     * @param line The line which is now at the top of the window.
     */
    void scrolled(int line);

    /**
     * Emitted when the selection is changed.
     */
    void selectionChanged();

	
private:
	int endWindowLine() const;
	void fillUnusedArea();
	int mapToScreen(int line) const;

	void getFilteredImage(Character* buffer,int size,int startLine,int endLine);
	void createFilterFolds(const QRegExp& filter);

    Screen* _screen; // see setScreen() , screen()
	Character* _windowBuffer;
	int _windowBufferSize;
	
	bool _bufferNeedsUpdate;
	
	enum FilterState
	{
		Current,
		NeedsPartialUpdate,
		NeedsFullUpdate
	};
	FilterState _filterState;

	int  _windowLines;
    int  _currentLine; // see scrollTo() , currentLine()
    bool _trackOutput; // see setTrackOutput() , trackOutput() 
    int  _scrollCount; // count of lines which the window has been scrolled by since
                       // the last call to resetScrollCount()

	Folds _folds;
	QRegExp _filter;
};



}
#endif // SCREENWINDOW_H
