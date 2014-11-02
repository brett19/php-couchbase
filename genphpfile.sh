#/bin/bash

echo "char *PCBC_PHP_CODESTR = \\" > phpstubstr.h
tail -n+2 "stub/constants.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/connstr.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/default_transcoder.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseViewQuery.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseN1qlQuery.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseCluster.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseClusterManager.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseBucket.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
tail -n+2 "stub/CouchbaseBucketManager.class.php" | sed 's/\\/\\\\/g' | sed 's/\"/\\\"/g' | sed 's/^\(.*\)$/\"\1\\n\" \\/' >> phpstubstr.h
echo "\"\";" >> phpstubstr.h
echo "Generated phpstubstr.h"
