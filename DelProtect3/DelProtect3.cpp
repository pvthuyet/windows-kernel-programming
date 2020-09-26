/*++

Module Name:

    DelProtect3.c

Abstract:

    This is the main module of the DelProtect3 miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include "DelProtect3.h"
#include "DelProtect3Common.h"
#include "..\include\FastMutex.h"
#include "..\include\AutoLock.h"
#include "kstring.h"

extern "C" NTSTATUS ZwQueryInformationProcess(
    _In_      HANDLE           ProcessHandle,
    _In_      PROCESSINFOCLASS ProcessInformationClass,
    _Out_     PVOID            ProcessInformation,
    _In_      ULONG            ProcessInformationLength,
    _Out_opt_ PULONG           ReturnLength
);

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

constexpr int MaxDirectories = 32;
DirectoryEntry DirNames[MaxDirectories];
int DirNamesCount;
FastMutex DirNamesLock;


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/*************************************************************************
    Prototypes
*************************************************************************/
int FindDirectory(_In_ PCUNICODE_STRING name, bool dosName);
NTSTATUS ConvertDosNameToNtName(_In_ PCWSTR dosName, _Out_ PUNICODE_STRING ntName);
bool IsDeleteAllowed(_In_ PFLT_CALLBACK_DATA Data);
void ClearAll();

EXTERN_C_START

DRIVER_DISPATCH DelProtect3CreateClose, DelProtect3DeviceControl;
DRIVER_UNLOAD DelProtect3UnloadDriver;

FLT_PREOP_CALLBACK_STATUS
DelProtect3PreCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS
DelProtect3PreSetInformation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
DelProtect3InstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
DelProtect3InstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
DelProtect3InstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
DelProtect3Unload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
DelProtect3InstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
DelProtect3PreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

VOID
DelProtect3OperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    );

FLT_POSTOP_CALLBACK_STATUS
DelProtect3PostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
DelProtect3PreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

BOOLEAN
DelProtect3DoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DelProtect3Unload)
#pragma alloc_text(PAGE, DelProtect3InstanceQueryTeardown)
#pragma alloc_text(PAGE, DelProtect3InstanceSetup)
#pragma alloc_text(PAGE, DelProtect3InstanceTeardownStart)
#pragma alloc_text(PAGE, DelProtect3InstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE, 0, DelProtect3PreCreate, nullptr },
    { IRP_MJ_SET_INFORMATION, 0, DelProtect3PreSetInformation, nullptr },

#if 0 // TODO - List all of the requests to filter.
    { IRP_MJ_CREATE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_CLOSE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_READ,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_WRITE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_QUERY_INFORMATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_QUERY_EA,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SET_EA,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      DelProtect3PreOperationNoPostOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_CLEANUP,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_PNP,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_MDL_READ,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      DelProtect3PreOperation,
      DelProtect3PostOperation },

#endif // TODO

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    DelProtect3Unload,                           //  MiniFilterUnload

    DelProtect3InstanceSetup,                    //  InstanceSetup
    DelProtect3InstanceQueryTeardown,            //  InstanceQueryTeardown
    DelProtect3InstanceTeardownStart,            //  InstanceTeardownStart
    DelProtect3InstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
DelProtect3InstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are always created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3InstanceSetup: Entered\n") );

    return STATUS_SUCCESS;
}


NTSTATUS
DelProtect3InstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

    If this routine is not defined in the registration structure, explicit
    detach requests via FltDetachVolume or FilterDetach will always be
    failed.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3InstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
DelProtect3InstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3InstanceTeardownStart: Entered\n") );
}


VOID
DelProtect3InstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3InstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DriverEntry: Entered\n") );

    // create a standard device object and symbolic link
    PDEVICE_OBJECT DeviceObject = nullptr;
    UNICODE_STRING devName = RTL_CONSTANT_STRING(DEVICE_NAME);
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYM_LINK_NAME);
    auto symLinkCreated = false;
    do
    {
        status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
        if (!NT_SUCCESS(status)) {
            break;
        }

        status = IoCreateSymbolicLink(&symLink, &devName);
        if (!NT_SUCCESS(status)) {
            break;
        }
        symLinkCreated = true;

        //
        //  Register with FltMgr to tell it our callback routines
        //

        status = FltRegisterFilter(DriverObject,
            &FilterRegistration,
            &gFilterHandle);
        FLT_ASSERT(NT_SUCCESS(status));
        if (!NT_SUCCESS(status)) {
            break;
        }

        DriverObject->DriverUnload = DelProtect3UnloadDriver;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = DelProtect3CreateClose;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DelProtect3DeviceControl;
        DirNamesLock.init();

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering(gFilterHandle);

    } while(false);

    if (!NT_SUCCESS(status))
    {
        if (gFilterHandle) {
            FltUnregisterFilter(gFilterHandle);
        }

        if (symLinkCreated) {
            IoDeleteSymbolicLink(&symLink);
        }

        if (DeviceObject) {
            IoDeleteDevice(DeviceObject);
        }
    }

    return status;
}

NTSTATUS
DelProtect3Unload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3Unload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
DelProtect3PreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3PreOperation: Entered\n") );

    //
    //  See if this is an operation we would like the operation status
    //  for.  If so request it.
    //
    //  NOTE: most filters do NOT need to do this.  You only need to make
    //        this call if, for example, you need to know if the oplock was
    //        actually granted.
    //

    if (DelProtect3DoRequestOperationStatus( Data )) {

        status = FltRequestOperationStatusCallback( Data,
                                                    DelProtect3OperationStatusCallback,
                                                    (PVOID)(++OperationStatusCtx) );
        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                          ("DelProtect3!DelProtect3PreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
                           status) );
        }
    }

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



VOID
DelProtect3OperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    )
/*++

Routine Description:

    This routine is called when the given operation returns from the call
    to IoCallDriver.  This is useful for operations where STATUS_PENDING
    means the operation was successfully queued.  This is useful for OpLocks
    and directory change notification operations.

    This callback is called in the context of the originating thread and will
    never be called at DPC level.  The file object has been correctly
    referenced so that you can access it.  It will be automatically
    dereferenced upon return.

    This is non-pageable because it could be called on the paging path

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    RequesterContext - The context for the completion routine for this
        operation.

    OperationStatus -

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3OperationStatusCallback: Entered\n") );

    PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                  ("DelProtect3!DelProtect3OperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                   OperationStatus,
                   RequesterContext,
                   ParameterSnapshot->MajorFunction,
                   ParameterSnapshot->MinorFunction,
                   FltGetIrpName(ParameterSnapshot->MajorFunction)) );
}


FLT_POSTOP_CALLBACK_STATUS
DelProtect3PostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3PostOperation: Entered\n") );

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
DelProtect3PreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("DelProtect3!DelProtect3PreOperationNoPostOperation: Entered\n") );

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
DelProtect3DoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    )
/*++

Routine Description:

    This identifies those operations we want the operation status for.  These
    are typically operations that return STATUS_PENDING as a normal completion
    status.

Arguments:

Return Value:

    TRUE - If we want the operation status
    FALSE - If we don't

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

    //
    //  return boolean state based on which operations we are interested in
    //

    return (BOOLEAN)

            //
            //  Check for oplock operations
            //

             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              //
              //    Check for directy change notification
              //

              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
               (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}


FLT_PREOP_CALLBACK_STATUS
DelProtect3PreCreate(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    if (Data->RequestorMode == KernelMode) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto& params = Data->Iopb->Parameters.Create;
    if (params.Options & FILE_DELETE_ON_CLOSE)
    {
        // delete operation
        KdPrint(("Delete on close: %wZ\n", &FltObjects->FileObject->FileName));
        if (!IsDeleteAllowed(Data)) {
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            return FLT_PREOP_COMPLETE;
        }
    }
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
DelProtect3PreSetInformation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    if (Data->RequestorMode == KernelMode) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto& params = Data->Iopb->Parameters.SetFileInformation;
    if (params.FileInformationClass != FileDispositionInformation 
        && params.FileInformationClass != FileDispositionInformationEx)
    {
        // not a delete operation
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto info = (FILE_DISPOSITION_INFORMATION*)params.InfoBuffer;
    if (!info->DeleteFile) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    if (IsDeleteAllowed(Data)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
    return FLT_PREOP_COMPLETE;
}

void DelProtect3UnloadDriver(PDRIVER_OBJECT DriverObject)
{
    ClearAll();
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYM_LINK_NAME);
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DelProtect3CreateClose(PDEVICE_OBJECT, PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DelProtect3DeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
    auto status = STATUS_SUCCESS;
    auto stack = IoGetCurrentIrpStackLocation(Irp);
    switch (stack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_DELPROTECT_ADD_DIR:
        {
            auto name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
            if (!name) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            auto bufferLen = stack->Parameters.DeviceIoControl.InputBufferLength;
            if (bufferLen > 1024) {
                // just too long for a directory
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            // make sure there is a NULL terminator somewhere
            auto maxStrLen = bufferLen / sizeof(WCHAR);
            name[maxStrLen - 1] = L'\0';
            auto dosNameLen = ::wcsnlen_s(name, maxStrLen);
            if (dosNameLen < 3) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            AutoLock locker(DirNamesLock);
            UNICODE_STRING strName;
            RtlInitUnicodeString(&strName, name);
            if (FindDirectory(&strName, true) >= 0) {
                break;
            }

            if (DirNamesCount == MaxDirectories) {
                status = STATUS_TOO_MANY_NAMES;
                break;
            }

            for (int i = 0; i < MaxDirectories; i++) 
            {
                if (DirNames[i].DosName.Buffer == nullptr)
                {
                    auto len = (dosNameLen + 2) * sizeof(WCHAR);
                    auto buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, len, DRIVER_TAG);
                    if (!buffer) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    }
                    ::wcscpy_s(buffer, len / sizeof(WCHAR), name);
                    
                    // append a backslash if it's missing
                    if (name[dosNameLen - 1] != L'\\') {
                        wcscat_s(buffer, dosNameLen + 2, L"\\");
                    }

                    status = ConvertDosNameToNtName(buffer, &DirNames[i].NtName);
                    if (!NT_SUCCESS(status)) {
                        ExFreePoolWithTag(buffer, DRIVER_TAG);
                        break;
                    }

                    RtlInitUnicodeString(&DirNames[i].DosName, buffer);
                    KdPrint(("Add: %wZ <=> %wZ\n", &DirNames[i].DosName, &DirNames[i].NtName));
                    ++DirNamesCount;
                    break;
                }
            }
        }
        break;

    case IOCTL_DELPROTECT_REMOVE_DIR:
        {
            auto name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
            if (!name) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            auto bufferLen = stack->Parameters.DeviceIoControl.InputBufferLength;
            if (bufferLen > 1024) {
                // just too long for a directory
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            auto maxStrLen = bufferLen / sizeof(WCHAR);
            // make sure there is a NULL terminator somewhere
            name[maxStrLen - 1] = L'\0';

            auto dosNameLen = ::wcsnlen_s(name, maxStrLen);
            if (dosNameLen < 3) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            AutoLock locker(DirNamesLock);
            UNICODE_STRING strName;
            RtlInitUnicodeString(&strName, name);
            int found = FindDirectory(&strName, true);
            if (found >= 0) 
            {
                DirNames[found].Free();
                DirNamesCount--;
            }
            else
            {
                status = STATUS_NOT_FOUND;
            }
        }
        break;

    case IOCTL_DELPROTECT_CLEAR:
        ClearAll();
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS ConvertDosNameToNtName(_In_ PCWSTR dosName, _Out_ PUNICODE_STRING ntName)
{
    if (!dosName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ntName->Buffer = nullptr;
    auto dosNameLen = ::wcsnlen_s(dosName, MAX_UNICODE_STACK_BUFFER_LENGTH);
    if (dosNameLen < 3)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // make sure we have a driver letter
    if (dosName[2] != L'\\' || dosName[1] != L':')
    {
        return STATUS_INVALID_PARAMETER;
    }

    kstring symLink(L"\\??\\", PagedPool, DRIVER_TAG);
    symLink.Append(dosName + 2, dosNameLen - 2); // driver letter and colon

    // prepare to open symbolic link
    UNICODE_STRING symLinkFull{ 0 };
    symLink.GetUnicodeString(&symLinkFull);
    OBJECT_ATTRIBUTES symLinkAttr{ 0 };
    InitializeObjectAttributes(&symLinkAttr, &symLinkFull, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    
    HANDLE hSymLink = nullptr;
    auto status = STATUS_SUCCESS;
    do
    {
        // open symbolic link
        status = ZwOpenSymbolicLinkObject(&hSymLink, GENERIC_READ, &symLinkAttr);
        if (!NT_SUCCESS(status))
        {
            break;
        }

        USHORT maxLen = 1024;
        ntName->Buffer = (wchar_t*)ExAllocatePoolWithTag(PagedPool, maxLen, DRIVER_TAG);
        if (!ntName->Buffer)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        ntName->MaximumLength = maxLen;

        // read target of symbolic link
        status = ZwQuerySymbolicLinkObject(hSymLink, ntName, nullptr);
        if (!NT_SUCCESS(status))
        {
            break;
        }

    } while (false);

    if (!NT_SUCCESS(status))
    {
        if (ntName->Buffer)
        {
            ExFreePool(ntName->Buffer);
            ntName->Buffer = nullptr;
            ntName->Length = ntName->MaximumLength = 0;
        }
    }
    else
    {
        RtlAppendUnicodeToString(ntName, dosName + 2);	// directory
    }
    if (hSymLink)
    {
        ZwClose(hSymLink);
    }

    return status;
}

int FindDirectory(_In_ PCUNICODE_STRING name, bool dosName)
{
    if (0 == DirNamesCount)
    {
        return -1;
    }

    for (int i = 0; i < MaxDirectories; i++)
    {
        const auto& dir = dosName ? DirNames[i].DosName : DirNames[i].NtName;
        if (dir.Buffer && RtlEqualUnicodeString(name, &dir, TRUE))
        {
            return i;
        }
    }

    return -1;
}

bool IsDeleteAllowed(_In_ PFLT_CALLBACK_DATA Data)
{
    PFLT_FILE_NAME_INFORMATION nameInfo = nullptr;
    auto allow = true;
    do
    {
        auto status = FltGetFileNameInformation(Data, FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &nameInfo);
        if (!NT_SUCCESS(status)) {
            break;
        }

        status = FltParseFileNameInformation(nameInfo);
        if (!NT_SUCCESS(status)) {
            break;
        }

        // concatenate volume+share+directory
        UNICODE_STRING path;
        path.Length = path.MaximumLength = nameInfo->Volume.Length + nameInfo->Share.Length + nameInfo->ParentDir.Length;
        path.Buffer = nameInfo->Volume.Buffer;
        KdPrint(("Checking directory: %wZ\n", &path));

        AutoLock locker(DirNamesLock);
        if (FindDirectory(&path, false) >= 0)
        {
            allow = false;
            KdPrint(("File not allowed to delete: %wZ\n", &nameInfo->Name));
        }

    } while (false);

    if (nameInfo)
    {
        FltReleaseFileNameInformation(nameInfo);
    }

    return allow;
}

void ClearAll()
{
    AutoLock locker(DirNamesLock);
    for (int i = 0; i < MaxDirectories; i++)
    {
        DirNames[i].Free();
    }
    DirNamesCount = 0;
}