<?php
/*
 * The following is a list of constants used for flags and datatype
 * encoding and decoding by the built in transcoders.
 */

/** @internal */ define('COUCHBASE_VAL_MASK', 0x1F);
/** @internal */ define('COUCHBASE_VAL_IS_STRING', 0);
/** @internal */ define('COUCHBASE_VAL_IS_LONG', 1);
/** @internal */ define('COUCHBASE_VAL_IS_DOUBLE', 2);
/** @internal */ define('COUCHBASE_VAL_IS_BOOL', 3);
/** @internal */ define('COUCHBASE_VAL_IS_SERIALIZED', 4);
/** @internal */ define('COUCHBASE_VAL_IS_IGBINARY', 5);
/** @internal */ define('COUCHBASE_VAL_IS_JSON', 6);
/** @internal */ define('COUCHBASE_COMPRESSION_MASK', 0x7 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_NONE', 0 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_ZLIB', 1 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_FASTLZ', 2 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_MCISCOMPRESSED', 1 << 4);
/** @internal */ define('COUCHBASE_SERTYPE_JSON', 0);
/** @internal */ define('COUCHBASE_SERTYPE_IGBINARY', 1);
/** @internal */ define('COUCHBASE_SERTYPE_PHP', 2);
/** @internal */ define('COUCHBASE_CMPRTYPE_NONE', 0);
/** @internal */ define('COUCHBASE_CMPRTYPE_ZLIB', 1);
/** @internal */ define('COUCHBASE_CMPRTYPE_FASTLZ', 2);
