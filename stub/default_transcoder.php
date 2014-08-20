<?php
/**
 * The default options for V1 encoding when using the default
 * transcoding functionality.
 * @internal
 */
$COUCHBASE_DEFAULT_ENCOPTS = array(
    'sertype' => COUCHBASE_SERTYPE_PHP,
    'cmprtype' => COUCHBASE_CMPRTYPE_NONE,
    'cmprthresh' => 2000,
    'cmprfactor' => 1.3
);

/**
 * Performs encoding of user provided types into binary form for
 * on the server according to the original PHP SDK specification.
 *
 * @internal
 *
 * @param $value The value passed by the user
 * @param $options Various encoding options
 * @return array An array specifying the bytes, flags and datatype to store
 */
function couchbase_basic_encoder_v1($value, $options) {
    $data = NULL;
    $flags = 0;
    $datatype = 0;

    $sertype = $options['sertype'];
    $cmprtype = $options['cmprtype'];
    $cmprthresh = $options['cmprthresh'];
    $cmprfactor = $options['cmprfactor'];

    $vtype = gettype($value);
    if ($vtype == 'string') {
        $flags = COUCHBASE_VAL_IS_STRING;
        $data = $value;
    } else if ($vtype == 'integer') {
        $flags = COUCHBASE_VAL_IS_LONG;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else if ($vtype == 'double') {
        $flags = COUCHBASE_VAL_IS_DOUBLE;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else if ($vtype == 'boolean') {
        $flags = COUCHBASE_VAL_IS_BOOL;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else {
        if ($sertype == COUCHBASE_SERTYPE_JSON) {
            $flags = COUCHBASE_VAL_IS_JSON;
            $datatype = COUCHBASE_VALUE_F_JSON;
            $data = json_encode($value);
        } else if ($sertype == COUCHBASE_SERTYPE_IGBINARY) {
            $flags = COUCHBASE_VAL_IS_IGBINARY;
            $data = igbinary_serialize($value);
        } else if ($sertype == COUCHBASE_SERTYPE_PHP) {
            $flags = COUCHBASE_VAL_IS_SERIALIZED;
            $data = serialize($value);
        }
    }

    if (strlen($data) < $cmprthresh) {
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    }

    if ($cmprtype != COUCHBASE_CMPRTYPE_NONE) {
        $cmprdata = NULL;
        $cmprflags = COUCHBASE_COMPRESSION_NONE;

        if ($cmprtype == COUCHBASE_CMPRTYPE_ZLIB) {
            $cmprdata = gzencode($data);
            $cmprflags = COUCHBASE_COMPRESSION_ZLIB;
        } else if ($cmprtype == COUCHBASE_CMPRTYPE_FASTLZ) {
            $cmprdata = fastlz_compress($data);
            $cmprflags = COUCHBASE_COMPRESSION_FASTLZ;
        }

        if ($cmprdata != NULL) {
            if (strlen($data) > strlen($cmprdata) * $cmprfactor) {
                $data = $cmprdata;
                $flags |= $cmprflags;
                $flags |= COUCHBASE_COMPRESSION_MCISCOMPRESSED;
            }
        }
    }

    return array($data, $flags, $datatype);
}

/**
 * Performs decoding of the server provided binary data into
 * PHP types according to the original PHP SDK specification.
 *
 * @internal
 *
 * @param $bytes The binary received from the server
 * @param $flags The flags received from the server
 * @param $datatype The datatype received from the server
 * @return mixed The resulting decoded value
 */
function couchbase_basic_decoder_v1($bytes, $flags, $datatype) {
    $sertype = $flags & COUCHBASE_VAL_MASK;
    $cmprtype = $flags & COUCHBASE_COMPRESSION_MASK;

    $data = $bytes;
    if ($cmprtype == COUCHBASE_COMPRESSION_ZLIB) {
        $bytes = gzdecode($bytes);
    } else if ($cmprtype == COUCHBASE_COMPRESSION_FASTLZ) {
        $data = fastlz_decompress($bytes);
    }

    $retval = NULL;
    if ($sertype == COUCHBASE_VAL_IS_STRING) {
        $retval = $data;
    } else if ($sertype == COUCHBASE_VAL_IS_LONG) {
        $retval = intval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_DOUBLE) {
        $retval = floatval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_BOOL) {
        $retval = boolval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_JSON) {
        $retval = json_decode($data);
    } else if ($sertype == COUCHBASE_VAL_IS_IGBINARY) {
        $retval = igbinary_unserialize($data);
    } else if ($sertype == COUCHBASE_VAL_IS_SERIALIZED) {
        $retval = unserialize($data);
    }

    return $retval;
}

/**
 * Default passthru encoder which simply passes data
 * as-is rather than performing any transcoding.
 *
 * @internal
 */
function couchbase_passthru_encoder($value) {
    return array($value, 0, 0);
}

/**
 * Default passthru encoder which simply passes data
 * as-is rather than performing any transcoding.
 *
 * @internal
 */
function couchbase_passthru_decoder($bytes, $flags, $datatype) {
    return $bytes;
}

/**
 * The default encoder for the client.  Currently invokes the
 * v1 encoder directly with the default set of encoding options.
 *
 * @internal
 */
function couchbase_default_encoder($value) {
    global $COUCHBASE_DEFAULT_ENCOPTS;
    return couchbase_basic_encoder_v1($value, $COUCHBASE_DEFAULT_ENCOPTS);
}

/**
 * The default decoder for the client.  Currently invokes the
 * v1 decoder directly.
 *
 * @internal
 */
function couchbase_default_decoder($bytes, $flags, $datatype) {
    return couchbase_basic_decoder_v1($bytes, $flags, $datatype);
}
