ktexteditor-mcursors
====================

Plugin-based software katepart that adds or removes virtual cursor in a document. All written or deleted text will be repeated on each slider.
mkdir build

### Several options are available

 - Synchronize cursors with virtual sliders.
 - Move between cursors
 - Disable cursor without deleting
 - Delete all the cursors or those located on the line

### If a selection is present

 - Add a cursor is for all lines of the selection
 - Removes all cursors only takes effect in the selection

Install
-------

```sh
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$(kde4-config --prefix)
make
sudo make install
```

or

```sh
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$(kde4-config --localprefix)
make
make install
```

Old version
-----------

 - 0.1: https://code.google.com/p/ktexteditor-mcursors/source/browse/#svn%2Ftrunk%2Ftag-0.1%253Fstate%253Dclosed
 - 0.2: https://code.google.com/p/ktexteditor-mcursors/source/browse/#svn%2Ftrunk%2Ftag-0.2%253Fstate%253Dclosed
