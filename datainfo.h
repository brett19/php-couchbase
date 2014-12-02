#ifndef DATATYPE_H_
#define DATATYPE_H_

enum {
    FMT_RESERVED = 0,
    FMT_PRIVATE = 1,
    FMT_JSON = 2,
    FMT_BINARY = 3,
    FMT_UTF8 = 4,

    FMT_MAX_VAL = 255,
    // PHP Private Types
    FMT_PHPSER,
    FMT_IGBINARY,
    FMT_STRING,
    FMT_LONG,
    FMT_DOUBLE,
    FMT_BOOL
} format_t;

enum {
    CPR_NONE = 0,
    CPR_GZIP = 1,
    CPR_BZIP = 2,
    CPR_LZO = 3,
    CPR_SNAPPY = 4,
    CPR_FASTLZ = 5,
    CPR_ZLIB = 6,

    CPR_MAX_VAL = 255
} compression_t;

enum {
    OLDFMT_STRING = 0,
    OLDFMT_LONG = 1,
    OLDFMT_DOUBLE = 2,
    OLDFMT_BOOL = 3,
    OLDFMT_SERIALIZED = 4,
    OLDFMT_IGBINARY = 5,
    OLDFMT_JSON = 6
} oldformat_t;
enum {
    OLDCPR_NONE = 0,
    OLDCPR_ZLIB = 1,
    OLDCPR_FASTLZ = 2
} oldcompression_t;

typedef struct datainfo_st {
    unsigned short format;
    unsigned short compression;
} datainfo_t;

static lcb_uint32_t make_oldflags(datainfo_t dt) {
    lcb_uint32_t out = 0;
    switch(dt.format) {
    case FMT_STRING: out |= ((OLDFMT_STRING<<0)&0x000F); break;
    case FMT_LONG: out |= ((OLDFMT_LONG<<0)&0x000F); break;
    case FMT_DOUBLE: out |= ((OLDFMT_DOUBLE<<0)&0x000F); break;
    case FMT_BOOL: out |= ((OLDFMT_BOOL<<0)&0x000F); break;
    case FMT_PHPSER: out |= ((OLDFMT_SERIALIZED<<0)&0x000F); break;
    case FMT_IGBINARY: out |= ((OLDFMT_IGBINARY<<0)&0x000F); break;
    case FMT_JSON: out |= ((OLDFMT_JSON<<0)&0x000F); break;
    }
    switch(dt.compression) {
    case CPR_NONE: out |= ((OLDCPR_NONE<<5)&0x00E0); break;
    case CPR_ZLIB: out |= ((OLDCPR_ZLIB<<5)&0x00E0); break;
    case CPR_FASTLZ: out |= ((OLDCPR_ZLIB<<5)&0x00E0); break;
    }
    out |= ((dt.format<<16) & 0x00FF0000);
    return out;
}

static datainfo_t parse_oldflags(lcb_uint32_t flags) {
    lcb_uint32_t format = (flags & 0x000F) >> 0;
    lcb_uint32_t compression = (flags & 0x00E0) >> 5;
    datainfo_t out = {0, 0};

    switch (format) {
    case OLDFMT_STRING: out.format = FMT_STRING; break;
    case OLDFMT_LONG: out.format = FMT_LONG; break;
    case OLDFMT_DOUBLE: out.format = FMT_DOUBLE; break;
    case OLDFMT_BOOL: out.format = FMT_BOOL; break;
    case OLDFMT_SERIALIZED: out.format = FMT_PHPSER; break;
    case OLDFMT_IGBINARY: out.format = FMT_IGBINARY; break;
    case OLDFMT_JSON: out.format = FMT_JSON; break;
    }
    switch (compression) {
    case OLDCPR_NONE: out.compression = CPR_NONE; break;
    case OLDCPR_ZLIB: out.compression = CPR_ZLIB; break;
    case OLDCPR_FASTLZ: out.compression = CPR_FASTLZ; break;
    }

    return out;
}

static lcb_uint8_t make_datatype(datainfo_t dt) {
    lcb_uint8_t value;
    if (dt.format > FMT_MAX_VAL) {
        value |= ((FMT_PRIVATE & 0x7) << 0);
    } else {
        value |= ((dt.format & 0x7) << 0);
    }
    value |= ((dt.compression & 0x7) << 5);
    return value;
}

static datainfo_t parse_datatype(lcb_uint8_t datatype) {
    datainfo_t out;
    out.format = ((datatype >> 0) & 0x7);
    out.compression = ((datatype >> 5) & 0x7);
    return out;
}

static lcb_uint32_t make_flags(datainfo_t dt) {
    lcb_uint32_t flags = 0;
    flags |= (make_oldflags(dt) << 0) & 0x000000FF;
    flags |= (make_datatype(dt) << 24) & 0xFF000000;
    return flags;
}

static datainfo_t get_datainfo(lcb_uint32_t flags, lcb_uint8_t datatype) {
    datainfo_t out;

    out = parse_datatype(datatype);
    if (out.format == 0 ) {
        out = parse_oldflags(flags);
    } else if (out.format == FMT_PRIVATE) {
        out.format = ((flags & 0x00FF0000) >> 16);
    }

    if (out.format == 0) {
        out.format = FMT_BINARY;
        out.compression = CPR_NONE;
    }

    return out;
}

#endif /* DATATYPE_H_ */
