#pragma once
#pragma warning(disable : 4127)

#if DBG

//
//  Debugging level flags.
//

#define KEDBG_TRACE_ROUTINES            0x00000001
#define KEDBG_TRACE_OPERATION_STATUS    0x00000002
#define KEDBG_TRACE_DEBUG               0x00000004
#define KEDBG_TRACE_ERROR               0x00000008
constexpr ULONG KEDBG_TRACE_FLAGS = KEDBG_TRACE_ROUTINES | KEDBG_TRACE_OPERATION_STATUS | KEDBG_TRACE_DEBUG | KEDBG_TRACE_ERROR;

#define KE_DBG_PRINT( _dbgLevel, _string )          \
    if(FlagOn(KEDBG_TRACE_FLAGS,(_dbgLevel))) {    \
        DbgPrint _string;                           \
    }

#else

#define KE_DBG_PRINT(_dbgLevel, _string)            {NOTHING;}

#endif

#define LOGENTER            KE_DBG_PRINT(KEDBG_TRACE_ROUTINES, (DRIVER_PREFIX __FUNCTION__ " enter {\n"))
#define LOGEXIT             KE_DBG_PRINT(KEDBG_TRACE_ROUTINES, (DRIVER_PREFIX __FUNCTION__ " exit }\n"))