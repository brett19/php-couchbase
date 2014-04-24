#/bin/bash

echo "char *PCBC_PHP_CODESTR = \\" > phpstubstr.h
tail -n+2 "Couchbase.class.php" | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
echo "\"\";" >> phpstubstr.h
echo "Generated phpstubstr.h"
