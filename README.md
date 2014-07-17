ktexteditor-mcursors
====================

Katepart plugin (Kwrite, Kate, Kdevelop, ...) that adds or deletes virtual cursors in a document.
Any text written or deleted will be repeated on each virtual cursor.

### Several options are available

 - Synchronize virtual cursors with the user cursor.
 - Move between virtual cursors.
 - Disable virtual cursors without deleting.
 - Delete all the virtual cursors or those located on the line.
 - Add a virtual cursor with ctrl+click (in plugin configuration).

### If a selection is present

 - Add a virtual cursor for each lines of the selection.
 - Removes all virtual cursors in the selection.

Install
-------

```sh
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$(kde4-config --prefix) -DQT_QMAKE_EXECUTABLE=/usr/bin/qmake-qt4
make
sudo make install
```

or

```sh
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$(kde4-config --localprefix) -DQT_QMAKE_EXECUTABLE=/usr/bin/qmake-qt4
make
make install
```

Old version
-----------

 - 0.1: https://code.google.com/p/ktexteditor-mcursors/source/browse/#svn%2Ftrunk%2Ftag-0.1%253Fstate%253Dclosed
 - 0.2: https://code.google.com/p/ktexteditor-mcursors/source/browse/#svn%2Ftrunk%2Ftag-0.2%253Fstate%253Dclosed
