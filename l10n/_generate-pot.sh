#!/bin/sh

#
# This script generates ../po/timedated.pot file
# and clean ../po/*.po files. After running this
# script we have to copy translated strings from
# ../po/*.po.back files.
#

cd ..
xgettext --keyword=N_ --keyword=_ --keyword=Q_:1,2  \
         --language=C --add-comments --sort-output  \
         --msgid-bugs-address="Andrey V.Kosteltsev <support@radix.pro>" \
         --package-name=timedated \
         --package-version=1.0.0 \
         --copyright-holder="Andrey V.Kosteltsev <kx@radix.pro>" \
         --default-domain=timedated --output=po/timedated.pot \
         `find -type f -name "*.c"`

( cd po ;
  mv ru_RU.utf8.po ru_RU.utf8.po.back
  msginit --no-translator --no-wrap --locale=ru_RU.utf8 --input=timedated.pot --output=ru_RU.utf8.po
)

cd l10n
