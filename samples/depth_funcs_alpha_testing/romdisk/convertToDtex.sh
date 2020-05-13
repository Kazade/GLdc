#! /bin/sh

FILE=$1
FILE_FLIP="$1_flip.png"
FILE_PATH=${FILE%/*}

echo $FILE_PATH

convert $FILE -flip $FILE_FLIP

$KOS_BASE/utils/texconv/texconv --in $FILE_FLIP --format ARGB1555 --preview $FILE_PATH/preview_1555.png --out $FILE_PATH/disk_1555.dtex
$KOS_BASE/utils/texconv/texconv --in $FILE_FLIP --format RGB565 --preview $FILE_PATH/preview_565.png --out $FILE_PATH/disk_565.dtex
$KOS_BASE/utils/texconv/texconv --in $FILE_FLIP --format ARGB4444 --preview $FILE_PATH/preview_4444.png --out $FILE_PATH/disk_4444.dtex

rm $FILE_FLIP

#rm $FILE_FLIP
