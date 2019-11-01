/*******************************************************************************
 (c) 2005-2019 Copyright, Real-Time Innovations, Inc.  All rights reserved.
 RTI grants Licensee a license to use, modify, compile, and create derivative
 works of the Software.  Licensee has the right to distribute object form only
 for use with RTI products.  The Software is provided "as is", with no warranty
 of any type, including any warranty for fitness for any purpose. RTI is under
 no obligation to maintain or support the Software.  RTI shall not be liable for
 any incidental or consequential damages arising out of the use or inability to
 use the software.
 ******************************************************************************/

#include <libgen.h>             // for basename()
#include <limits.h>
#include <wchar.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "out_dds_str.h"
#include <cJSON.h> 

#define PLUGIN_NAME         "out_dds_str"
#define MAX_XML_FILE        10

// Boolean to String:
static const char * const BOOLEAN_STRING_TRUE   = "TRUE";
static const char * const BOOLEAN_STRING_FALSE  = "FALSE";

static const wchar_t * const BOOLEAN_WSTRING_TRUE   = L"TRUE";
static const wchar_t * const BOOLEAN_WSTRING_FALSE  = L"FALSE";

static const uint64_t MAX_UINT64_TO_SHORT = RTIXCdrShort_MAX;           // +0x7fff
static const int64_t  MAX_INT64_TO_SHORT  = RTIXCdrShort_MAX;           // +0x7fff
static const int64_t  MIN_INT64_TO_SHORT  = RTIXCdrShort_MIN;           // -0x8000

static const uint64_t MAX_UINT64_TO_USHORT = RTIXCdrUnsignedShort_MAX;  // +0xffff
static const int64_t  MAX_INT64_TO_USHORT  = RTIXCdrUnsignedShort_MAX;  // +0xffff

static const uint64_t MAX_UINT64_TO_LONG = RTIXCdrLong_MAX;     // +0x7fffffff
static const int64_t  MAX_INT64_TO_LONG  = RTIXCdrLong_MAX;     // +0x7fffffff
static const int64_t  MIN_INT64_TO_LONG  = RTIXCdrLong_MIN;     // 00x80000000

static const uint64_t MAX_UINT64_TO_ULONG = RTIXCdrUnsignedLong_MAX;  // +0xffffffff
static const int64_t  MAX_INT64_TO_ULONG  = RTIXCdrUnsignedLong_MAX;  // +0xffffffff

static const uint64_t MAX_UINT64_TO_FLOAT = (1<<24) + 1;              // 24 = mantissa for 32-bit float
static const int64_t  MAX_INT64_TO_FLOAT  = (1<<24) + 1;
static const int64_t  MIN_INT64_TO_FLOAT  = -(1<<24) - 1;             // XXX: Is this true?

static const uint64_t MAX_UINT64_TO_DOUBLE = (1LLU<<53) + 1;          // 53 = mantissa for 64-bit double
static const int64_t  MAX_INT64_TO_DOUBLE  = (1LL<<53) + 1;
static const int64_t  MIN_INT64_TO_DOUBLE  = -(1LL<<24) - 1;          // XXX: Is this true?

static const uint64_t MAX_UINT64_TO_OCTET = RTIXCdrOctet_MAX;  // +0xff
static const int64_t  MAX_INT64_TO_OCTET  = RTIXCdrOctet_MAX;  // +0xff

static const uint64_t MAX_UINT64_TO_LONGLONG  = RTIXCdrLongLong_MAX;

// Stringification...
// I.e.:
//      #define FOO     5
//      RTI_STR(FOO)   -> "FOO"
//      RTI_XSTR(FOO)  -> "5"
//
#define RTI_XSTR(s)             RTI_STR(s)
#define RTI_STR(s)              #s

#define OOM_CHECK(ptr)      if (!(ptr)) do { flb_error("OUT OF MEMORY: " RTI_XSTR(ptr)); abort(); } while(0)


/* {{{ dds_shutdown
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static int dds_shutdown(DDS_DomainParticipant *participant)
{
    int status = 0;
    DDS_ReturnCode_t retcode;

    if (participant != NULL) {
        retcode = DDS_DomainParticipant_delete_contained_entities(participant);
        if (retcode != DDS_RETCODE_OK) {
            flb_error("[%s] delete_contained_entities error %d\n", PLUGIN_NAME, retcode);
            status = -1;
        }
        retcode = DDS_DomainParticipantFactory_delete_participant(
            DDS_TheParticipantFactory, participant);
        if (retcode != DDS_RETCODE_OK) {
            flb_error("[%s] delete_participant error %d\n", PLUGIN_NAME, retcode);
            status = -1;
        }
    }
    /* RTI Data Distribution Service provides the finalize_instance() method on
    domain participant factory for users who want to release memory used
    by the participant factory. Uncomment the following block of code for
    clean destruction of the singleton. */
    /*
    retcode = DDS_DomainParticipantFactory_finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        printf("finalize_instance error %d\n", retcode);
        status = -1;
    }
    */
    return status;
}

// }}}
#if 0
/* {{{ getMemberKind
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Returns DDS_TK_NULL in case of error
 */
static DDS_TCKind getMemberKind(DDS_TypeCode *typeCode, const char * memberName) {
    char *ptr;
    DDS_ExceptionCode_t err;
    DDS_UnsignedLong memberId;

    ptr = strchr(memberName, '.');
    if (!ptr) {
        DDS_TypeCode *memberTC;
        DDS_TCKind retVal;
        // memberName is not a complex nested name
        memberId = DDS_TypeCode_find_member_by_name(typeCode, memberName, &err);
        if (err != DDS_NO_EXCEPTION_CODE) {
            flb_error("Error: cannot find type code kind for member '%s': %d", memberName, err);
            return DDS_TK_NULL;
        }
        memberTC = DDS_TypeCode_getMemberType(typeCode, memberId, &err);
        if (err != DDS_NO_EXCEPTION_CODE) {
            flb_error("Error: cannot retrieve member type for '%s': %d", memberName, err);
            return DDS_TK_NULL;
        }
        retVal = DDS_TypeCode_kind(memberTC, &err);
        if (err != DDS_NO_EXCEPTION_CODE) {
            flb_error("Error: cannot retrieve type code kind for member '%s': %d", memberName, err);
            return DDS_TK_NULL;
        }
        return retVal;
    }

    // Else, memberName identify a nested entity: 'foo.bar.zoo'


        



// }}}
#endif

/* {{{ readFile
 * -------------------------------------------------------------------------
 */
char * readFile(const char *path) {
    RTIBool ok = RTI_FALSE;
    FILE *f = NULL;
    struct stat fs;
    char *retVal = NULL;

    assert(path);

    if (stat(path, &fs)) {
        flb_warn("Failed to read type map file: %s", path);
        return NULL;
    }

    retVal = (char *)malloc(fs.st_size);
    OOM_CHECK(retVal);

    f = fopen(path, "r");
    if (!f) {
        // Hmm how come stat didn't fail?
        flb_warn("Error opening type map file: %s (error=%s, errno=%d)", path, strerror(errno), errno);
        goto done;
    }

    if (fread(retVal, 1, (size_t)fs.st_size, f) < (size_t)fs.st_size) {
        flb_warn("Error reading type map file: %s (errno=%d)", strerror(errno), errno);
        goto done;
    }

    retVal[fs.st_size] = '\0';
    ok = RTI_TRUE;

done:
    if (f) {
        fclose(f);
    }
    if (!ok && retVal) {
        free(retVal);
        retVal = NULL;
    }
    return retVal;
}
/* }}} */

/* {{{ msgpack_object_initFromString
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline void msgpack_object_initFromString(msgpack_object *me, const char *str) {
    me->type = MSGPACK_OBJECT_STR;
    me->via.str.ptr = str;
    me->via.str.size = strlen(str);
}

// }}}
/* {{{ msgpack_object_toString
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Given a MSGPACK_OBJECT_STR builds a NULL-terminated copy of the string.
 * Returned pointer must be freed.
 * Call abort() in case of out of memory
 */
static inline char * msgpack_object_toString(const msgpack_object *me) {
    char *retVal = calloc(1, me->via.str.size+1);
    OOM_CHECK(retVal);
    memcpy(retVal, me->via.str.ptr, me->via.str.size);
    return retVal;
}

// }}}

/* {{{ PRECISION_LOSS_WARNING
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Returns DDS_TK_NULL in case of error
 */
static inline void PRECISION_LOSS_WARNING(struct flb_out_dds_str_config *ctx, const char *from, const char *to, const char *memberName) {
    switch(ctx->pcAction) {
        case PrecisionLossAction_NONE:
            break;

        case PrecisionLossAction_WARN_ONCE:
            ctx->pcAction = PrecisionLossAction_NONE;
            // AND YES, STUPID COMPILER, FALLING INTO THE NEXT CASE IS NOT AN ERROR!

        case PrecisionLossAction_WARN:
            flb_warn("precision loss mapping '%s' -> '%s' for member '%s'", from, to, memberName);
            break;

        case PrecisionLossAction_ABORT:
            flb_error("precision loss mapping '%s' -> '%s' for member '%s'. Aborting operation...", from, to, memberName);
            abort();
    }
}

// }}}
/* {{{ transformValue_Short
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Short transformValue_Short(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Short target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_SHORT) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "short", memberName);
            }
            target = (DDS_Short)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_SHORT) || (value->via.i64 < MIN_INT64_TO_SHORT)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "short", memberName);
            }
            target = (DDS_Short)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "short", memberName);
            target = (DDS_Short)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 7) {      // Longest string can be either "+0x7fff" or "-65535"
                PRECISION_LOSS_WARNING(ctx, "string", "short", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_Short)strtol(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "short", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "short", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_UnsignedShort
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_UnsignedShort transformValue_UnsignedShort(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_UnsignedShort target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_USHORT) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "ushort", memberName);
            }
            target = (DDS_UnsignedShort)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_USHORT) || (value->via.i64 < 0)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "ushort", memberName);
            }
            target = (DDS_UnsignedShort)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "short", memberName);
            target = (DDS_UnsignedShort)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 7) {      // Longest string can be either "+0x7fff" or "+65535"
                PRECISION_LOSS_WARNING(ctx, "string", "ushort", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_UnsignedShort)strtoul(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "ushort", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "ushort", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Long
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Long transformValue_Long(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Long target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_LONG) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "long", memberName);
            }
            target = (DDS_Long)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_LONG) || (value->via.i64 < MIN_INT64_TO_LONG)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "long", memberName);
            }
            target = (DDS_Long)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "long", memberName);
            target = (DDS_Long)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 12) {      // Longest string can be either "+0x7fffffff" or "-2147483647"
                PRECISION_LOSS_WARNING(ctx, "string", "long", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_Long)strtol(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "long", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "long", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_UnsignedLong
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_UnsignedLong transformValue_UnsignedLong(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_UnsignedLong target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_ULONG) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "ulong", memberName);
            }
            target = (DDS_UnsignedLong)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_ULONG) || (value->via.i64 < 0)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "ulong", memberName);
            }
            target = (DDS_UnsignedLong)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "long", memberName);
            target = (DDS_UnsignedLong)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 12) {      // Longest string can be either "+0xffffffff" or "+4294967295"
                PRECISION_LOSS_WARNING(ctx, "string", "ulong", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_UnsignedLong)strtoul(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "ulong", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "ulong", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Float
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Float transformValue_Float(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Float target = 0.0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1.0 : 0.0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_FLOAT) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "float", memberName);
            }
            target = (DDS_Float)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_FLOAT) || (value->via.i64 < MIN_INT64_TO_FLOAT)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "float", memberName);
            }
            target = (DDS_Float)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "float", memberName);
            target = (DDS_Float)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_Float)strtof(buf, &ptr);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "float", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "float", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Double
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Double transformValue_Double(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Double target = 0.0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1.0 : 0.0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_DOUBLE) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "double", memberName);
            }
            target = (DDS_Double)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_DOUBLE) || (value->via.i64 < MIN_INT64_TO_DOUBLE)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "double", memberName);
            }
            target = (DDS_Double)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "double", memberName);
            target = (DDS_Double)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_Double)strtof(buf, &ptr);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "double", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "double", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Boolean
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Boolean transformValue_Boolean(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Boolean target = DDS_BOOLEAN_FALSE;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            target = (value->via.u64 != 0) ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            target = (value->via.i64 != 0) ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            // This most likely is not going to work...
            target = (value->via.f64 != 0.0) ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
            break;

        case MSGPACK_OBJECT_STR: 
            if ( ((value->via.str.size == 4) && !strncasecmp(value->via.str.ptr, BOOLEAN_STRING_TRUE, 4)) ||
                 ((value->via.str.size == 1) && (*value->via.str.ptr == '1')) ) {
                target = DDS_BOOLEAN_TRUE;
            }
            if ( ((value->via.str.size == 5) && !strncasecmp(value->via.str.ptr, BOOLEAN_STRING_FALSE, 5)) ||
                 ((value->via.str.size == 1) && (*value->via.str.ptr == '0')) ) {
                target = DDS_BOOLEAN_FALSE;
            }
            PRECISION_LOSS_WARNING(ctx, "string", "boolean", memberName);
            target = (value->via.str.size == 0) ? DDS_BOOLEAN_TRUE : DDS_BOOLEAN_FALSE;
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "boolean", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Char
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Char transformValue_Char(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Char target = '\0';
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? '1' : '0';
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            // Map number into char '0..9' using UTF-8/ASCII
            if (value->via.u64 > 9) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "char", memberName);
            }
            target = '0' + (value->via.u64 % 10);
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            // Map number into char '0..9' using UTF-8/ASCII
            if ((value->via.i64 > 9) || (value->via.i64 < 0)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "char", memberName);
            }
            if (value->via.i64 < 0) {
                target = '0' - (value->via.i64 % 10);
            } else {
                target = '0' + (value->via.i64 % 10);
            }
            break;

        case MSGPACK_OBJECT_FLOAT64:
            // This is not going to work...
            PRECISION_LOSS_WARNING(ctx, "double", "char", memberName);
            target = '\0';
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 1) {
                PRECISION_LOSS_WARNING(ctx, "string", "char", memberName);
            }
            if (value->via.str.size) {
                target = *value->via.str.ptr;
            } else {
                target = '\0';
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "char", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Wchar
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Wchar transformValue_Wchar(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Wchar target = '\0';
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? '1' : '0';
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            // Map number into wchar '0..9' using UTF-8/ASCII
            if (value->via.u64 > 9) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "wchar", memberName);
            }
            target = '0' + (value->via.u64 % 10);
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            // Map number into wchar '0..9' using UTF-8/ASCII
            if ((value->via.i64 > 9) || (value->via.i64 < 0)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "wchar", memberName);
            }
            if (value->via.i64 < 0) {
                target = '0' - (value->via.i64 % 10);
            } else {
                target = '0' + (value->via.i64 % 10);
            }
            break;

        case MSGPACK_OBJECT_FLOAT64:
            // This is not going to work...
            PRECISION_LOSS_WARNING(ctx, "double", "wchar", memberName);
            target = '\0';
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 1) {
                PRECISION_LOSS_WARNING(ctx, "string", "wchar", memberName);
            }
            if (value->via.str.size) {
                target = (DDS_Wchar)*value->via.str.ptr;
            } else {
                target = '\0';
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "wchar", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_Octet
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_Octet transformValue_Octet(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_Octet target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_OCTET) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "octet", memberName);
            }
            target = (DDS_Octet)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if ((value->via.i64 > MAX_INT64_TO_OCTET) || (value->via.i64 < 0)) {
                PRECISION_LOSS_WARNING(ctx, "int64", "octet", memberName);
            }
            target = (DDS_Octet)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "octet", memberName);
            target = (DDS_Octet)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 5) {      // Longest string can be either "+0x7f" or "-128"
                PRECISION_LOSS_WARNING(ctx, "string", "octet", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_Octet)strtoul(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "octet", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "octet", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_String
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * caller must free allocated string
 */
static inline char * transformValue_String(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    char *target = NULL;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = strdup(value->via.boolean ? BOOLEAN_STRING_TRUE : BOOLEAN_STRING_FALSE);
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            {   // C99: use snprintf to calculate the correct length
                int len = snprintf(NULL, 0, "%lu", value->via.u64) + 1;
                target = malloc(len);
                OOM_CHECK(target);
                snprintf(target, len, "%lu", value->via.u64);
                break;
            }

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            {   // C99: use snprintf to calculate the correct length
                int len = snprintf(NULL, 0, "%ld", value->via.i64) + 1;
                target = malloc(len);
                OOM_CHECK(target);
                snprintf(target, len, "%ld", value->via.i64);
                break;
            }

        case MSGPACK_OBJECT_FLOAT64:
            {   // C99: use snprintf to calculate the correct length
                int len = snprintf(NULL, 0, "%f", value->via.f64) + 1;
                target = malloc(len);
                OOM_CHECK(target);
                snprintf(target, len, "%f", value->via.f64);
                break;
            }

        case MSGPACK_OBJECT_STR: 
            target = malloc(value->via.str.size+1);
            OOM_CHECK(target);
            memcpy(target, value->via.str.ptr, value->via.str.size);
            target[value->via.str.size] = '\0';
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "string", memberName);
            target = strdup("N/A");
    }
    return target;
}

// }}}
/* {{{ transformValue_Wstring
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * caller must free allocated buffer
 */
static inline wchar_t * transformValue_Wstring(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    wchar_t *target = NULL;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = wcsdup(value->via.boolean ? BOOLEAN_WSTRING_TRUE : BOOLEAN_WSTRING_FALSE);
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            {
                int len = swprintf(NULL, 0, L"%lu", value->via.u64) + 1;
                target = (wchar_t *)malloc(len);
                OOM_CHECK(target);
                swprintf(target, len, L"%lu", value->via.u64);
                break;
            }

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            {
                int len = swprintf(NULL, 0, L"%ld", value->via.i64) + 1;
                target = (wchar_t *)malloc(len);
                OOM_CHECK(target);
                swprintf(target, len, L"%ld", value->via.i64);
                break;
            }

        case MSGPACK_OBJECT_FLOAT64:
            {
                int len = swprintf(NULL, 0, L"%f", value->via.f64) + 1;
                target = (wchar_t *)malloc(len);
                OOM_CHECK(target);
                swprintf(target, len, L"%f", value->via.f64);
                break;
            }

        case MSGPACK_OBJECT_STR: 
            target = calloc(value->via.str.size*2+1, sizeof(wchar_t));
            OOM_CHECK(target);
            mbstowcs(target, value->via.str.ptr, value->via.str.size);
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "string", memberName);
            target = wcsdup(L"N/A");
    }
    return target;
}

// }}}
/* {{{ transformValue_LongLong
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_LongLong transformValue_LongLong(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_LongLong target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            if (value->via.u64 > MAX_UINT64_TO_LONGLONG) {
                PRECISION_LOSS_WARNING(ctx, "uint64", "longlong", memberName);
            }
            target = (DDS_LongLong)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            target = (DDS_LongLong)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "longlong", memberName);
            target = (DDS_LongLong)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 20) {      // 20 Longest string
                PRECISION_LOSS_WARNING(ctx, "string", "longlong", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_LongLong)strtoll(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "longlong", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "longlong", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_UnsignedLongLong
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline DDS_UnsignedLongLong transformValue_UnsignedLongLong(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_UnsignedLongLong target = 0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1 : 0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            target = (DDS_UnsignedLongLong)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            if (value->via.i64 < 0) {
                PRECISION_LOSS_WARNING(ctx, "int64", "ulonglong", memberName);
            }
            target = (DDS_UnsignedLongLong)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "ulonglong", memberName);
            target = (DDS_UnsignedLongLong)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            if (value->via.str.size > 20) {      // Longest string
                PRECISION_LOSS_WARNING(ctx, "string", "ulonglong", memberName);
            } else {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = (DDS_UnsignedLongLong)strtoull(buf, &ptr, 0);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "ulonglong", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "ulonglong", memberName);
    }
    return target;
}

// }}}
/* {{{ transformValue_LongDouble
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static inline long double transformValue_LongDouble(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    long double target = 0.0;
    switch(value->type) {
        case MSGPACK_OBJECT_BOOLEAN:
            target = value->via.boolean ? 1.0 : 0.0;
            break;

        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            // This platform dependent, so we print a warning all the time
            PRECISION_LOSS_WARNING(ctx, "uint64", "longdouble", memberName);
            target = (long double)value->via.u64;
            break;

        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            PRECISION_LOSS_WARNING(ctx, "int64", "longdouble", memberName);
            target = (long double)value->via.i64;
            break;

        case MSGPACK_OBJECT_FLOAT64:
            PRECISION_LOSS_WARNING(ctx, "double", "longdouble", memberName);
            target = (long double)value->via.f64;
            break;

        case MSGPACK_OBJECT_STR: 
            {
                char *ptr;
                char *buf = msgpack_object_toString(value);
                target = strtold(buf, &ptr);
                if (*ptr) {
                    PRECISION_LOSS_WARNING(ctx, "string", "longdouble", memberName);
                } 
                free(buf);
            }
            break;

        // MSGPACK_OBJECT_NIL, MSGPACK_OBJECT_EXT, MSGPACK_OBJECT_ARRAY, MSGPACK_OBJECT_MAP, MSGPACK_OBJECT_BIN
        default:    
            PRECISION_LOSS_WARNING(ctx, "non-primitive", "longdouble", memberName);
    }
    return target;
}

// }}}

/* {{{ setValueToMember
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Returns DDS_BOOLEAN_FALSE if the operation failed.
 * Note: ctx is not const because it might modify the pcAction property (WARN_ONCE -> NONE)
 */
static DDS_Boolean setValueToMember(struct flb_out_dds_str_config *ctx, const msgpack_object *value, const char *memberName) {
    DDS_DynamicData *instance = ctx->instance;
    struct DDS_DynamicDataMemberInfo memberInfo;
    DDS_ReturnCode_t rc = DDS_DynamicData_get_member_info(instance, &memberInfo, memberName, DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
    
    if (rc != DDS_RETCODE_OK) {
        flb_warn("Error: cannot retrieve type info for member '%s': %d", memberName, rc);
        return DDS_BOOLEAN_FALSE;
    }
    // First exclude all the FluentBit types that we cannot handle correctly
    if ((value->type == MSGPACK_OBJECT_NIL) ||
        (value->type == MSGPACK_OBJECT_BIN) ||
        (value->type == MSGPACK_OBJECT_EXT) ||
        (value->type == MSGPACK_OBJECT_ARRAY) ||
        (value->type == MSGPACK_OBJECT_MAP)) {
        flb_error("Error: cannot map FluentBit type %d for member '%s'", value->type, memberName);
        return DDS_BOOLEAN_FALSE;
    }

    // First go through the DDS type and then inner select the source type (flatbit)
    switch(memberInfo.member_kind) {
        case DDS_TK_SHORT:
            DDS_DynamicData_set_short(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Short(ctx, value, memberName)); 
            break;

        case DDS_TK_ENUM:
        case DDS_TK_LONG:
            DDS_DynamicData_set_long(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Long(ctx, value, memberName)); 
            break;

        case DDS_TK_USHORT:
            DDS_DynamicData_set_ushort(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_UnsignedShort(ctx, value, memberName)); 
            break;

        case DDS_TK_ULONG:
            DDS_DynamicData_set_ulong(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_UnsignedLong(ctx, value, memberName)); 
            break;
        case DDS_TK_FLOAT:
            DDS_DynamicData_set_float(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Float(ctx, value, memberName)); 
            break;

        case DDS_TK_DOUBLE:
            DDS_DynamicData_set_double(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Double(ctx, value, memberName)); 
            break;

        case DDS_TK_BOOLEAN:
            DDS_DynamicData_set_boolean(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Boolean(ctx, value, memberName)); 
            break;

        case DDS_TK_CHAR:
            DDS_DynamicData_set_char(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Char(ctx, value, memberName)); 
            break;

        case DDS_TK_WCHAR:
            DDS_DynamicData_set_wchar(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Wchar(ctx, value, memberName)); 
            break;

        case DDS_TK_OCTET:
            DDS_DynamicData_set_octet(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_Octet(ctx, value, memberName)); 
            break;

        case DDS_TK_STRING:
            {
                char *ptr = transformValue_String(ctx, value, memberName); 
                DDS_DynamicData_set_string(instance, 
                        memberName, 
                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                        ptr);
                free(ptr);
                break;
            }

        case DDS_TK_WSTRING:
            {
                DDS_Wchar *ptr = (DDS_Wchar *)transformValue_Wstring(ctx, value, memberName); 
                DDS_DynamicData_set_wstring(instance, 
                        memberName, 
                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                        ptr);
                free(ptr);
                break;
            }

        case DDS_TK_LONGLONG:
            DDS_DynamicData_set_longlong(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_LongLong(ctx, value, memberName)); 
            break;

        case DDS_TK_ULONGLONG:
            DDS_DynamicData_set_ulonglong(instance, 
                    memberName, 
                    DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                    transformValue_UnsignedLongLong(ctx, value, memberName)); 
            break;

        case DDS_TK_LONGDOUBLE: 
            {
                DDS_LongDouble ddsVal;
                long double val = transformValue_LongDouble(ctx, value, memberName); 

                // TODO: Is this the right way to convert a long double?
                memcpy(&ddsVal, &val, sizeof(ddsVal));

                DDS_DynamicData_set_longdouble(instance, 
                        memberName, 
                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, 
                        ddsVal);
                break;
            }

        case DDS_TK_STRUCT:
        case DDS_TK_UNION:
        case DDS_TK_SEQUENCE:
        case DDS_TK_ARRAY:
        case DDS_TK_ALIAS:
            flb_error("Error: member '%s' is not a primitive type", memberName);
            return DDS_BOOLEAN_FALSE;

        case DDS_TK_NULL:
        default:
            flb_error("Error: member '%s' is of an unknown type: %d", memberInfo.member_kind);
            return DDS_BOOLEAN_FALSE;
    }
    return DDS_BOOLEAN_TRUE;
}

// }}}

/* {{{ mapObjectToType
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Returns RTI_TRUE if mapping was performed correctly and sample can be published
 */
static RTIBool mapObjectToType(const char *tag, struct flb_out_dds_str_config *ctx, const msgpack_object *obj) {
    size_t i;
    cJSON *topMap;
    cJSON *tagNode;
    cJSON *mapNode;
    cJSON *staticNode;
    cJSON *node;
    msgpack_object key;
    msgpack_object value;
    char keyStr[50];

    assert(tag);
    assert(ctx);
    assert(obj);

    for (topMap = ctx->typeMap->child; topMap; topMap = topMap->next) {
        if (topMap->type != cJSON_Object) {
            flb_warn("Unexpected element of type map array: not an object (%d), ignored", topMap->type);
            continue;
        }
        tagNode = cJSON_GetObjectItem(topMap, "tag");
        if (!tagNode) {
            flb_warn("Missing 'tag' property from type map object (skipping it)");
            continue;
        }
        if (strcmp(tagNode->valuestring, tag)) {
            // No match, skip
            continue;
        }

        // 'map' property is required
        mapNode = cJSON_GetObjectItem(topMap, "map");
        if (!mapNode) {
            // No map provided
            flb_error("No map provided for tag: '%s'", tag);
            return RTI_FALSE;
        }
        if (mapNode->type != cJSON_Object) {
            flb_error("Wrong type for 'map' (%d) property for tag: '%s'", mapNode->type, tag);
            return RTI_FALSE;
        }

        // 'static' property is optional
        staticNode = cJSON_GetObjectItem(topMap, "static");
        if (staticNode && (staticNode->type != cJSON_Object)) {
            flb_error("Wrong type for 'static' (%d) property for tag: '%s'", staticNode->type, tag);
            return RTI_FALSE;
        }

        // Process map
        for (i = 0; i < obj->via.map.size; i++) {
            key = obj->via.map.ptr[i].key;
            value = obj->via.map.ptr[i].val;

            memset(keyStr, 0, sizeof(keyStr));
            strncpy(keyStr, key.via.str.ptr, key.via.str.size);

            // Do we have a mapping property?
            node = cJSON_GetObjectItem(mapNode, keyStr);
            if (!node) {
                // not found in map
                continue;
            }
            // expect node to be a string
            if (node->type != cJSON_String) {
                flb_error("Wrong type for property '%s' (%d) in map for tag '%s'", node->string, node->type, tag);
                return RTI_FALSE;
            }

            // Success, copy value using dynamic data
            if (!setValueToMember(ctx, &value, node->valuestring)) {
                flb_warn("failed to assign value for key='%s' in tag='%s'", node->string, tag);
            }
        }

        if (staticNode) {
            // Now process the static section
            msgpack_object temp;     // Just a temporary msgpack object to hold the static value
            for (node = staticNode->child; node; node=node->next) {
                switch(node->type) {
                    case cJSON_Number: {
                        double tempDouble;
                        // Can be an integer?
                        if (modf(node->valuedouble, &tempDouble) == 0.0) {
                            temp.type = MSGPACK_OBJECT_NEGATIVE_INTEGER;
                            temp.via.i64 = tempDouble;
                        } else {
                            temp.type = MSGPACK_OBJECT_FLOAT64;
                            temp.via.f64 = node->valuedouble;
                        }
                        break;
                    }
                    case cJSON_True:
                    case cJSON_False:
                        temp.type = MSGPACK_OBJECT_BOOLEAN;
                        temp.via.boolean = (node->type == cJSON_True);
                        break;

                    case cJSON_String:
                        msgpack_object_initFromString(&temp, node->valuestring);
                        break;

                    default:
                        flb_warn("Cannot map static property '%s': unsupported type (%d)", node->string, node->type);
                        continue;
                }
                if (!setValueToMember(ctx, &temp, node->string)) {
                    flb_warn("failed to assign value for static entry='%s' in tag='%s'", node->string, tag);
                }
            }
        }
        return RTI_TRUE;
    }

    flb_warn("No type map found for tag '%s' (ignored)", tag);
    return RTI_FALSE;
}

// }}}

/* {{{ cb_dds_exit
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static int cb_dds_exit(void *data, struct flb_config *config) {
    struct flb_out_dds_str_config *ctx = data;
    if (ctx) {
        if (ctx->participant) {
            dds_shutdown(ctx->participant);
        }
        flb_free(ctx);
    }
    return 0;
}

// }}}
/* {{{ cb_dds_init
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static int cb_dds_init(struct flb_output_instance *ins,
		struct flb_config *config,
		void *data) {
    struct flb_out_dds_str_config *ctx;
    const char *tmp = NULL;
    const char *arg_DomainParticipant;
    const char *arg_DataWriter;
    DDS_ReturnCode_t rc;
    const char *arg_XMLFile[MAX_XML_FILE];
    int arg_XMLFileCount = 0;
    int i;
    char fileCountKey[20];

    ctx = flb_calloc(1, sizeof(struct flb_out_dds_str_config));
    if (!ctx) {
        flb_errno();
        goto err;
    }

    // Read configuration parameters
    memset(arg_XMLFile, 0, sizeof(arg_XMLFile));

    ctx->pcAction = DEFAULT_PRECISION_LOSS_ACTION;
    tmp = flb_output_get_property("PrecisionLossAction", ins);
    if (tmp) {
        if (!strcmp(tmp, "none")) {
            ctx->pcAction = PrecisionLossAction_NONE;
        } else if (!strcmp(tmp, "warn")) {
            ctx->pcAction = PrecisionLossAction_WARN;
        } else if (!strcmp(tmp, "warn_once")) {
            ctx->pcAction = PrecisionLossAction_WARN_ONCE;
        } else if (!strcmp(tmp, "abort")) {
            ctx->pcAction = PrecisionLossAction_ABORT;
        } else {
            flb_warn("unrecognized value for config property 'precision_loss_action': %s. Using default", tmp);
        }
    }
    
    // Type map
    tmp = flb_output_get_property("TypeMap", ins);
    if (!tmp) {
        flb_error("Missing type map file");
        goto err;
    } else {
        char *ff = readFile(tmp);
        if (!ff) {
            flb_error("Unable to read provided type map file");
            goto err;
        }
        ctx->typeMap = cJSON_Parse(ff);
        free(ff);
        if (!ctx->typeMap) {
            flb_error("Failed to parse type map JSON file");
            goto err;
        }
        // Top node must be an array
        if (ctx->typeMap->type != cJSON_Array) {
            cJSON_Delete(ctx->typeMap);
            flb_error("Top-element of type map is not an Array: %d", ctx->typeMap->type);
            goto err;
        }
    }
    
    // Plain XMLFile only?
    arg_XMLFile[0] = flb_output_get_property("XMLFile", ins);
    if (arg_XMLFile[0]) {
        arg_XMLFileCount = 1;
    } else {
        // Attempt to read the XMLFile_0, XML_File_1 ...
        for (arg_XMLFileCount = 0; arg_XMLFileCount < MAX_XML_FILE; ++arg_XMLFileCount) {
            snprintf(fileCountKey, sizeof(fileCountKey), "XMLFile_%d", arg_XMLFileCount);
            arg_XMLFile[arg_XMLFileCount] = flb_output_get_property(fileCountKey, ins);
            if (!arg_XMLFile[arg_XMLFileCount]) {
                // Don't scan for all the keys, as soon as we don't find a key stop
                break;
            }
        }
        if (arg_XMLFileCount == MAX_XML_FILE) {
            flb_warn("This plugin support only %d XMLFile_xx keys", MAX_XML_FILE);
        }
    }

    arg_DomainParticipant = flb_output_get_property("DomainParticipant", ins);
    if (!arg_DomainParticipant) {
        flb_error("Missing required parameter 'DomainParticipant'");
        goto err;
    }

    arg_DataWriter = flb_output_get_property("DataWriter", ins);
    if (!arg_DataWriter) {
        flb_error("Missing required parameter 'DataWriter'");
        goto err;
    }

    if (arg_XMLFileCount > 0) {
        // Add the given XML file to the list of files to automatically load
        struct DDS_DomainParticipantFactoryQos factoryQos = DDS_DomainParticipantFactoryQos_INITIALIZER;

        if ((rc = DDS_DomainParticipantFactory_get_qos(DDS_TheParticipantFactory, &factoryQos)) != DDS_RETCODE_OK) {
            flb_error("Unable to get participant factory QoS: %d", rc);
            goto err;
        }
        if (!DDS_StringSeq_from_array(&factoryQos.profile.url_profile, arg_XMLFile, arg_XMLFileCount)) {
            flb_error("Failed to copy XML file list from array");
            goto err;
        }
        if ((rc = DDS_DomainParticipantFactory_set_qos(DDS_TheParticipantFactory, &factoryQos)) != DDS_RETCODE_OK) {
            flb_error("Unable to set participant factory QoS: %d", rc);
            goto err;
        }
    }

    ctx->participant = DDS_DomainParticipantFactory_create_participant_from_config(DDS_TheParticipantFactory, arg_DomainParticipant);
    if (!ctx->participant) {
        flb_error("Failed to create domain participant '%s'", arg_DomainParticipant);
        goto err;
    }
    
    ctx->writer = DDS_DynamicDataWriter_narrow(DDS_DomainParticipant_lookup_datawriter_by_name(ctx->participant, arg_DataWriter));
    if (!ctx->writer) {
        flb_error("Cannot find data writer '%s' in domain participant", arg_DataWriter);
        goto err;
    }

    ctx->typeCode = DDS_DomainParticipant_get_typecode(ctx->participant, "CIM_Malware_Attacks");
    if (!ctx->typeCode) {
        flb_error("Failed to get typecode");
        goto err;
    }
    ctx->typeSupport = DDS_DynamicDataTypeSupport_new(ctx->typeCode, &DDS_DYNAMIC_DATA_TYPE_PROPERTY_DEFAULT);
    if (!ctx->typeSupport) {
        flb_error("Failed to get type support");
        goto err;
    }
    ctx->instance = DDS_DynamicDataTypeSupport_create_data(ctx->typeSupport);
    if (!ctx->instance) {
        flb_error("Failed to create dynamic data instance");
        goto err;
    }

    // TODO: Add support for keyed types
    ctx->instance_handle = DDS_HANDLE_NIL;

    flb_output_set_context(ins, ctx);
    flb_info("[%s] Successfully created writer %s::%s", PLUGIN_NAME, arg_DomainParticipant, arg_DataWriter);
    return 0;

err:
    cb_dds_exit(ctx, NULL);
    return -1;
}

// }}}
/* {{{ cb_dds_flush
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static void cb_dds_flush(const void *data, 
        size_t bytes,
        const char *tag, 
        int tag_len,
        struct flb_input_instance *i_ins,
        void *out_context,
        struct flb_config *config) {

    struct flb_out_dds_str_config *ctx = (struct flb_out_dds_str_config *)out_context;
    size_t off = 0;
    msgpack_unpacked result;
    msgpack_object *obj;
    msgpack_object key;
    msgpack_object value;
    struct flb_time tms;            // Time of message
    size_t i;
    char keyStr[50];
    size_t count = 0;
    DDS_ReturnCode_t rc;

    while(msgpack_unpack_next(&result, data, bytes, &off) == MSGPACK_UNPACK_SUCCESS) {
        flb_time_pop_from_msgpack(&tms, &result, &obj);
        
        if (mapObjectToType(tag, ctx, obj)) {
            rc = DDS_DynamicDataWriter_write(ctx->writer, ctx->instance, &DDS_HANDLE_NIL);
            if (rc != DDS_RETCODE_OK) {
                flb_error("Failed to write DDS sample: %d", rc);
            } else {
                ++count;
            }
        }

#if 0
        if (!strncmp(tag, "mcafee.found", tag_len)) {
            /* Current mapping
                 FluentBit          Attacks
                 --------------------------------------------------
                                -> action_enum = AttackAction_Blocked
               ◇ time           ->
               ◇ hostname       -> dest.host
               ◇ severity       -> 
               ◇ appName        -> vendor_product
               ◇ pid            -> 
               ◇ filepath       -> file_path, file_name
               ◇ filesize       -> 
               ◇ virusname      -> signature
               ◇ scantime       -> date
               ◇ processname    -> 
               ◇ username       ->
               ◇ profiletype    ->
            */

            for (i = 0; i < obj->via.map.size; i++) {
                key = obj->via.map.ptr[i].key;
                value = obj->via.map.ptr[i].val;

                memset(keyStr, 0, sizeof(keyStr));
                strncpy(keyStr, key.via.str.ptr, key.via.str.size);

                if (!strcmp(keyStr, "hostname")) {
                    setValueToMember(ctx, &value, "dest.host");
                } else if (!strcmp(keyStr, "appName")) {
                    setValueToMember(ctx, &value, "vendor_product");
                } else if (!strcmp(keyStr, "filepath")) {
                    // 'filepath' is used for both file_path and file_name
                    char *base;
                    char *path;                     // Need to make a copy of the value to invoke 'basename'...
                    msgpack_object fileNameObj;     // ...and place it in a temp msgpack_object
                    setValueToMember(ctx, &value, "file_path");

                    path = msgpack_object_toString(&value);
                    base = basename(path);
                    msgpack_object_initFromString(&fileNameObj, base);
                    setValueToMember(ctx, &fileNameObj, "file_name");
                    free(path);

                } else if (!strcmp(keyStr, "virusname")) {
                    setValueToMember(ctx, &value, "signature");
                } else if (!strcmp(keyStr, "scantime")) {
                    setValueToMember(ctx, &value, "date.sec");
                }
            }
            rc = DDS_DynamicDataWriter_write(ctx->writer, ctx->instance, &DDS_HANDLE_NIL);
            if (rc != DDS_RETCODE_OK) {
                flb_error("Failed to write DDS sample: %d", rc);
            } else {
                ++count;
            }
        } else {
            flb_warn("Ignoring unhandled tag: %s", tag);
        }
#endif
    } // while there are messages to flush

    msgpack_unpacked_destroy(&result);
    FLB_OUTPUT_RETURN(FLB_OK);
    flb_debug("Written %d DDS samples...", count);
}

// }}}


struct flb_output_plugin out_dds_str_plugin = {
	.name         = "dds_str",
	.description  = "DDS Structured Output Plugin",
	.cb_init      = cb_dds_init,
	.cb_flush     = cb_dds_flush,
	.cb_exit      = cb_dds_exit,
	.flags        = 0
};
