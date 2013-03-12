#!/bin/zsh

SUBDIRS="App Book Data File Items Scenes ResourceMagic"

cd `dirname $0`
cd ../src

echo "# Automatically generated by updatesources.sh" > eln.pri
echo "# on " `date` >> eln.pri
echo "" >> eln.pri
echo "sourcedirs = \\" >> eln.pri

for DIR in $=SUBDIRS; do
    echo "    $DIR \\" >> eln.pri

    echo "# Automatically generated by updatesources.sh" > $DIR/$DIR.pri
    echo "# on " `date` >> $DIR/$DIR.pri
    echo "" >> $DIR/$DIR.pri
    echo "HEADERS += \\" >> $DIR/$DIR.pri
    for H in $DIR/*.H; do
	echo "    $H \\" >> $DIR/$DIR.pri
    done
    echo "" >> $DIR/$DIR.pri

    echo "SOURCES += \\" >> $DIR/$DIR.pri
    for C in $DIR/*.C; do
	echo "    $C \\" >> $DIR/$DIR.pri
    done
    echo "" >> $DIR/$DIR.pri

    echo "RESOURCES += \\" >> $DIR/$DIR.pri
    for QRC in $DIR/*.qrc(.N); do
	echo "    $QRC \\" >> $DIR/$DIR.pri
    done
    echo "" >> $DIR/$DIR.pri
done

echo "" >> eln.pri
