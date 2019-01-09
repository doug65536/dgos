#pragma once

#include "types.h"

extern "C" void halt(tchar const *s);

/*
 * Portions Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.
 * Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
 * This source in this file are licensed and made available under the terms
 * and conditions of the BSD License. The full text of the license may be
 * found at http://opensource.org/licenses/bsd-license.php.
 */


#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249
#define EFI_SYSTEM_TABLE_REVISION ((1<<16) | (10))
#define EFI_1_10_SYSTEM_TABLE_REVISION ((1<<16) | (10))
#define EFI_1_02_SYSTEM_TABLE_REVISION ((1<<16) | (02))

#define IN
#define OUT
#define OPTIONAL

typedef bool BOOLEAN;
typedef intptr_t INTN;
typedef uintptr_t UINTN;
typedef int8_t INT8;
typedef uint8_t UINT8;
typedef int16_t INT16;
typedef uint16_t UINT16;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef char CHAR8;
typedef char16_t CHAR16;
typedef void VOID;
typedef INTN EFI_STATUS;
typedef VOID* EFI_HANDLE;
typedef VOID* EFI_EVENT;
typedef UINT64 EFI_LBA;
typedef UINTN EFI_TPL;
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

struct EFI_MAC_ADDRESS {
    char addr[32];
};

struct EFI_IPv4_ADDRESS {
    char addr[4];
};

struct EFI_IPv6_ADDRESS {
    char addr[16];
};

union EFI_IP_ADDRESS {
    EFI_IPv4_ADDRESS ipv4;
    EFI_IPv6_ADDRESS ipv6;
};

struct EFI_GUID {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
};

#define EFIAPI __attribute__((__ms_abi__))

struct EFI_TABLE_HEADER {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
};

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL SIMPLE_TEXT_OUTPUT_INTERFACE;

struct SIMPLE_INPUT_INTERFACE;
struct EFI_RUNTIME_SERVICES;
struct EFI_BOOT_SERVICES;
struct EFI_CONFIGURATION_TABLE;
struct EFI_BLOCK_IO_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
struct EFI_FILE_PROTOCOL;

///
/// Protocol name defined in EFI1.1.
///
typedef EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   EFI_FILE_IO_INTERFACE;
typedef EFI_FILE_PROTOCOL                 EFI_FILE;

struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    SIMPLE_INPUT_INTERFACE *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut;
    EFI_HANDLE StandardErrorHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
};

#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42
#define EFI_BOOT_SERVICES_REVISION ((1<<16) | (10))

//*******************************************************
// EFI_TPL
//*******************************************************
typedef UINTN EFI_TPL;

//*******************************************************
// Task Priority Levels
//*******************************************************
#define TPL_APPLICATION 4
#define TPL_CALLBACK 8
#define TPL_NOTIFY 16
#define TPL_HIGH_LEVEL 31

//*******************************************************
// EFI_EVENT
//*******************************************************
typedef VOID *EFI_EVENT;

//*******************************************************
// Event Types
//*******************************************************
// These types can be “ORed” together as needed – for example, 
// EVT_TIMER might be “Ored” with EVT_NOTIFY_WAIT or 
// EVT_NOTIFY_SIGNAL.
#define EVT_TIMER 0x80000000
#define EVT_RUNTIME  0x40000000
#define EVT_NOTIFY_WAIT  0x00000100
#define EVT_NOTIFY_SIGNAL  0x00000200
#define EVT_SIGNAL_EXIT_BOOT_SERVICES 0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE 0x60000202

/**
  Raises a task's priority level and returns its previous level.
  @param[in]  NewTpl          The new task priority level.
  @return Previous task priority level
**/
typedef
EFI_TPL
(EFIAPI *EFI_RAISE_TPL)(
        IN EFI_TPL      NewTpl
        );

/**
  Restores a task's priority level to its previous value.
  @param[in]  OldTpl          The previous task priority level to restore.
**/
typedef
VOID
(EFIAPI *EFI_RESTORE_TPL)(
        IN EFI_TPL      OldTpl
        );

enum EFI_ALLOCATE_TYPE {
    ///
    /// Allocate any available range of pages that satisfies the request.
    ///
    AllocateAnyPages,
    ///
    /// Allocate any available range of pages whose uppermost address is
    /// less than or equal to a specified maximum address.
    ///
    AllocateMaxAddress,
    ///
    /// Allocate pages at a specified address.
    ///
    AllocateAddress,
    ///
    /// Maximum enumeration value that may be used for bounds checking.
    ///
    MaxAllocateType
};

enum EFI_MEMORY_TYPE {
    ///
    /// Not used.
    ///
    EfiReservedMemoryType,
    ///
    /// The code portions of a loaded application.
    /// (Note that UEFI OS loaders are UEFI applications.)
    ///
    EfiLoaderCode,
    ///
    /// The data portions of a loaded application and the default data
    /// allocation type used by an application to allocate pool memory.
    ///
    EfiLoaderData,
    ///
    /// The code portions of a loaded Boot Services Driver.
    ///
    EfiBootServicesCode,
    ///
    /// The data portions of a loaded Boot Serves Driver, and the default data
    /// allocation type used by a Boot Services Driver to allocate pool memory.
    ///
    EfiBootServicesData,
    ///
    /// The code portions of a loaded Runtime Services Driver.
    ///
    EfiRuntimeServicesCode,
    ///
    /// The data portions of a loaded Runtime Services Driver and the default
    /// data allocation type used by a Runtime Services Driver to allocate
    /// pool memory.
    ///
    EfiRuntimeServicesData,
    ///
    /// Free (unallocated) memory.
    ///
    EfiConventionalMemory,
    ///
    /// Memory in which errors have been detected.
    ///
    EfiUnusableMemory,
    ///
    /// Memory that holds the ACPI tables.
    ///
    EfiACPIReclaimMemory,
    ///
    /// Address space reserved for use by the firmware.
    ///
    EfiACPIMemoryNVS,
    ///
    /// Used by system firmware to request that a memory-mapped IO region
    /// be mapped by the OS to a virtual address so it can be accessed by EFI
    /// runtime services.
    ///
    EfiMemoryMappedIO,
    ///
    /// System memory-mapped IO region that is used to translate memory
    /// cycles to IO cycles by the processor.
    ///
    EfiMemoryMappedIOPortSpace,
    ///
    /// Address space reserved by the firmware for code that is part of
    /// the processor.
    ///
    EfiPalCode,
    ///
    /// A memory region that operates as EfiConventionalMemory,
    /// however it happens to also support byte-addressable non-volatility.
    ///
    EfiPersistentMemory,
    EfiMaxMemoryType
};

/**
  Allocates memory pages from the system.
  @param[in]       Type         The type of allocation to perform.
  @param[in]       MemoryType   The type of memory to allocate.
                                MemoryType values in the range
                                0x70000000..0x7FFFFFFF
                                are reserved for OEM use.
                                MemoryType values in the range
                                0x80000000..0xFFFFFFFF are reserved for use by
                                UEFI OS loaders that are provided by operating
                                system vendors.
  @param[in]       Pages        The number of contiguous 4 KB pages to allocate.
  @param[in, out]  Memory       The pointer to a physical address. On input,
                                the way in which the address is
                                used depends on the value of Type.
  @retval EFI_SUCCESS           The requested pages were allocated.
  @retval EFI_INVALID_PARAMETER 1) Type is not AllocateAnyPages or
                                AllocateMaxAddress or AllocateAddress.
                                2) MemoryType is in the range
                                EfiMaxMemoryType..0x6FFFFFFF.
                                3) Memory is NULL.
                                4) MemoryType is EfiPersistentMemory.
  @retval EFI_OUT_OF_RESOURCES  The pages could not be allocated.
  @retval EFI_NOT_FOUND         The requested pages could not be found.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_PAGES)(
    IN     EFI_ALLOCATE_TYPE            Type,
    IN     EFI_MEMORY_TYPE              MemoryType,
    IN     UINTN                        Pages,
    IN OUT EFI_PHYSICAL_ADDRESS         *Memory
    );

/**
  Frees memory pages.
  @param[in]  Memory      The base physical address of the pages to be freed.
  @param[in]  Pages       The number of contiguous 4 KB pages to free.
  @retval EFI_SUCCESS           The requested pages were freed.
  @retval EFI_INVALID_PARAMETER Memory is not a page-aligned address or Pages
                                is invalid.
  @retval EFI_NOT_FOUND         The requested memory pages were not allocated
                                with AllocatePages().
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FREE_PAGES)(
    IN  EFI_PHYSICAL_ADDRESS         Memory,
    IN  UINTN                        Pages
    );

struct EFI_MEMORY_DESCRIPTOR {
    ///
    /// Type of the memory region.  See EFI_MEMORY_TYPE.
    ///
    UINT32                Type;
    ///
    /// Physical address of the first byte of the memory region.  Must aligned
    /// on a 4 KB boundary.
    ///
    EFI_PHYSICAL_ADDRESS  PhysicalStart;
    ///
    /// Virtual address of the first byte of the memory region.  Must aligned
    /// on a 4 KB boundary.
    ///
    EFI_VIRTUAL_ADDRESS   VirtualStart;
    ///
    /// Number of 4KB pages in the memory region.
    ///
    UINT64                NumberOfPages;
    ///
    /// Attributes of the memory region that describe the bit mask of
    /// capabilities for that memory region, and not necessarily the current
    /// settings for that memory region.
    ///
    UINT64                Attribute;
};

/**
  Returns the current memory map.
  @param[in, out]  MemoryMapSize         A pointer to the size, in bytes, of the MemoryMap buffer.
                                         On input, this is the size of the buffer allocated by the caller.
                                         On output, it is the size of the buffer returned by the firmware if
                                         the buffer was large enough, or the size of the buffer needed to contain
                                         the map if the buffer was too small.
  @param[in, out]  MemoryMap             A pointer to the buffer in which firmware places the current memory
                                         map.
  @param[out]      MapKey                A pointer to the location in which firmware returns the key for the
                                         current memory map.
  @param[out]      DescriptorSize        A pointer to the location in which firmware returns the size, in bytes, of
                                         an individual EFI_MEMORY_DESCRIPTOR.
  @param[out]      DescriptorVersion     A pointer to the location in which firmware returns the version number
                                         associated with the EFI_MEMORY_DESCRIPTOR.
  @retval EFI_SUCCESS           The memory map was returned in the MemoryMap buffer.
  @retval EFI_BUFFER_TOO_SMALL  The MemoryMap buffer was too small. The current buffer size
                                needed to hold the memory map is returned in MemoryMapSize.
  @retval EFI_INVALID_PARAMETER 1) MemoryMapSize is NULL.
                                2) The MemoryMap buffer is not too small and MemoryMap is
                                   NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_MEMORY_MAP)(
    IN OUT UINTN                       *MemoryMapSize,
    IN OUT EFI_MEMORY_DESCRIPTOR       *MemoryMap,
    OUT    UINTN                       *MapKey,
    OUT    UINTN                       *DescriptorSize,
    OUT    UINT32                      *DescriptorVersion
    );

/**
  Allocates pool memory.
  @param[in]   PoolType         The type of pool to allocate.
                                MemoryType values in the range 0x70000000..0x7FFFFFFF
                                are reserved for OEM use. MemoryType values in the range
                                0x80000000..0xFFFFFFFF are reserved for use by UEFI OS loaders
                                that are provided by operating system vendors.
  @param[in]   Size             The number of bytes to allocate from the pool.
  @param[out]  Buffer           A pointer to a pointer to the allocated buffer if the call succeeds;
                                undefined otherwise.
  @retval EFI_SUCCESS           The requested number of bytes was allocated.
  @retval EFI_OUT_OF_RESOURCES  The pool requested could not be allocated.
  @retval EFI_INVALID_PARAMETER Buffer is NULL.
                                PoolType is in the range EfiMaxMemoryType..0x6FFFFFFF.
                                PoolType is EfiPersistentMemory.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_POOL)(
    IN  EFI_MEMORY_TYPE              PoolType,
    IN  UINTN                        Size,
    OUT VOID                         **Buffer
    );

/**
  Returns pool memory to the system.
  @param[in]  Buffer            The pointer to the buffer to free.
  @retval EFI_SUCCESS           The memory was returned to the system.
  @retval EFI_INVALID_PARAMETER Buffer was invalid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FREE_POOL)(
    IN  VOID                         *Buffer
    );

/**
  Invoke a notification event
  @param[in]  Event                 Event whose notification function is being invoked.
  @param[in]  Context               The pointer to the notification function's context,
                                    which is implementation-dependent.
**/
typedef
VOID
(EFIAPI *EFI_EVENT_NOTIFY)(
    IN  EFI_EVENT                Event,
    IN  VOID                     *Context
    );

/**
  Creates an event.
  @param[in]   Type             The type of event to create and its mode and attributes.
  @param[in]   NotifyTpl        The task priority level of event notifications, if needed.
  @param[in]   NotifyFunction   The pointer to the event's notification function, if any.
  @param[in]   NotifyContext    The pointer to the notification function's context; corresponds to parameter
                                Context in the notification function.
  @param[out]  Event            The pointer to the newly created event if the call succeeds; undefined
                                otherwise.
  @retval EFI_SUCCESS           The event structure was created.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The event could not be allocated.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREATE_EVENT)(
    IN  UINT32                       Type,
    IN  EFI_TPL                      NotifyTpl,
    IN  EFI_EVENT_NOTIFY             NotifyFunction,
    IN  VOID                         *NotifyContext,
    OUT EFI_EVENT                    *Event
    );

enum EFI_TIMER_DELAY {
    ///
    /// An event's timer settings is to be cancelled and not trigger time is to be set/
    ///
    TimerCancel,
    ///
    /// An event is to be signaled periodically at a specified interval from the current time.
    ///
    TimerPeriodic,
    ///
    /// An event is to be signaled once at a specified interval from the current time.
    ///
    TimerRelative
};

/**
  Sets the type of timer and the trigger time for a timer event.
  @param[in]  Event             The timer event that is to be signaled at the specified time.
  @param[in]  Type              The type of time that is specified in TriggerTime.
  @param[in]  TriggerTime       The number of 100ns units until the timer expires.
                                A TriggerTime of 0 is legal.
                                If Type is TimerRelative and TriggerTime is 0, then the timer
                                event will be signaled on the next timer tick.
                                If Type is TimerPeriodic and TriggerTime is 0, then the timer
                                event will be signaled on every timer tick.
  @retval EFI_SUCCESS           The event has been set to be signaled at the requested time.
  @retval EFI_INVALID_PARAMETER Event or Type is not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SET_TIMER)(
    IN  EFI_EVENT                Event,
    IN  EFI_TIMER_DELAY          Type,
    IN  UINT64                   TriggerTime
    );

/**
  Stops execution until an event is signaled.
  @param[in]   NumberOfEvents   The number of events in the Event array.
  @param[in]   Event            An array of EFI_EVENT.
  @param[out]  Index            The pointer to the index of the event which satisfied the wait condition.
  @retval EFI_SUCCESS           The event indicated by Index was signaled.
  @retval EFI_INVALID_PARAMETER 1) NumberOfEvents is 0.
                                2) The event indicated by Index is of type
                                   EVT_NOTIFY_SIGNAL.
  @retval EFI_UNSUPPORTED       The current TPL is not TPL_APPLICATION.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_WAIT_FOR_EVENT)(
    IN  UINTN                    NumberOfEvents,
    IN  EFI_EVENT                *Event,
    OUT UINTN                    *Index
    );

/**
  Signals an event.
  @param[in]  Event             The event to signal.
  @retval EFI_SUCCESS           The event has been signaled.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIGNAL_EVENT)(
    IN  EFI_EVENT                Event
    );

/**
  Closes an event.
  @param[in]  Event             The event to close.
  @retval EFI_SUCCESS           The event has been closed.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CLOSE_EVENT)(
    IN EFI_EVENT                Event
    );

/**
  Checks whether an event is in the signaled state.
  @param[in]  Event             The event to check.
  @retval EFI_SUCCESS           The event is in the signaled state.
  @retval EFI_NOT_READY         The event is not in the signaled state.
  @retval EFI_INVALID_PARAMETER Event is of type EVT_NOTIFY_SIGNAL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CHECK_EVENT)(
    IN EFI_EVENT                Event
    );

enum EFI_INTERFACE_TYPE {
    ///
    /// Indicates that the supplied protocol interface is supplied in native form.
    ///
    EFI_NATIVE_INTERFACE
};

/**
  Installs a protocol interface on a device handle. If the handle does not exist, it is created and added
  to the list of handles in the system. InstallMultipleProtocolInterfaces() performs
  more error checking than InstallProtocolInterface(), so it is recommended that
  InstallMultipleProtocolInterfaces() be used in place of
  InstallProtocolInterface()
  @param[in, out]  Handle         A pointer to the EFI_HANDLE on which the interface is to be installed.
  @param[in]       Protocol       The numeric ID of the protocol interface.
  @param[in]       InterfaceType  Indicates whether Interface is supplied in native form.
  @param[in]       Interface      A pointer to the protocol interface.
  @retval EFI_SUCCESS           The protocol interface was installed.
  @retval EFI_OUT_OF_RESOURCES  Space for a new handle could not be allocated.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER InterfaceType is not EFI_NATIVE_INTERFACE.
  @retval EFI_INVALID_PARAMETER Protocol is already installed on the handle specified by Handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(
    IN OUT EFI_HANDLE               *Handle,
    IN     EFI_GUID         const   *Protocol,
    IN     EFI_INTERFACE_TYPE       InterfaceType,
    IN     VOID                     *Interface
    );

/**
  Reinstalls a protocol interface on a device handle.
  @param[in]  Handle            Handle on which the interface is to be reinstalled.
  @param[in]  Protocol          The numeric ID of the interface.
  @param[in]  OldInterface      A pointer to the old interface. NULL can be used if a structure is not
                                associated with Protocol.
  @param[in]  NewInterface      A pointer to the new interface.
  @retval EFI_SUCCESS           The protocol interface was reinstalled.
  @retval EFI_NOT_FOUND         The OldInterface on the handle was not found.
  @retval EFI_ACCESS_DENIED     The protocol interface could not be reinstalled,
                                because OldInterface is still being used by a
                                driver that will not release it.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE)(
    IN EFI_HANDLE               Handle,
    IN EFI_GUID        const    *Protocol,
    IN VOID                     *OldInterface,
    IN VOID                     *NewInterface
    );

/**
  Removes a protocol interface from a device handle. It is recommended that
  UninstallMultipleProtocolInterfaces() be used in place of
  UninstallProtocolInterface().
  @param[in]  Handle            The handle on which the interface was installed.
  @param[in]  Protocol          The numeric ID of the interface.
  @param[in]  Interface         A pointer to the interface.
  @retval EFI_SUCCESS           The interface was removed.
  @retval EFI_NOT_FOUND         The interface was not found.
  @retval EFI_ACCESS_DENIED     The interface was not removed because the interface
                                is still being used by a driver.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(
    IN EFI_HANDLE               Handle,
    IN EFI_GUID         const   *Protocol,
    IN VOID                     *Interface
    );

/**
  Queries a handle to determine if it supports a specified protocol.
  @param[in]   Handle           The handle being queried.
  @param[in]   Protocol         The published unique identifier of the protocol.
  @param[out]  Interface        Supplies the address where a pointer to the corresponding Protocol
                                Interface is returned.
  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the specified protocol.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_HANDLE_PROTOCOL)(
    IN  EFI_HANDLE               Handle,
    IN  EFI_GUID          const  *Protocol,
    OUT VOID                     **Interface
    );

/**
  Creates an event that is to be signaled whenever an interface is installed for a specified protocol.
  @param[in]   Protocol         The numeric ID of the protocol for which the event is to be registered.
  @param[in]   Event            Event that is to be signaled whenever a protocol interface is registered
                                for Protocol.
  @param[out]  Registration     A pointer to a memory location to receive the registration value.
  @retval EFI_SUCCESS           The notification event has been registered.
  @retval EFI_OUT_OF_RESOURCES  Space for the notification event could not be allocated.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER Event is NULL.
  @retval EFI_INVALID_PARAMETER Registration is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY)(
    IN  EFI_GUID       const     *Protocol,
    IN  EFI_EVENT                Event,
    OUT VOID                     **Registration
    );

enum EFI_LOCATE_SEARCH_TYPE {
    ///
    /// Retrieve all the handles in the handle database.
    ///
    AllHandles,
    ///
    /// Retrieve the next handle fron a RegisterProtocolNotify() event.
    ///
    ByRegisterNotify,
    ///
    /// Retrieve the set of handles from the handle database that support a
    /// specified protocol.
    ///
    ByProtocol
};

/**
  Returns an array of handles that support a specified protocol.
  @param[in]       SearchType   Specifies which handle(s) are to be returned.
  @param[in]       Protocol     Specifies the protocol to search by.
  @param[in]       SearchKey    Specifies the search key.
  @param[in, out]  BufferSize   On input, the size in bytes of Buffer. On output, the size in bytes of
                                the array returned in Buffer (if the buffer was large enough) or the
                                size, in bytes, of the buffer needed to obtain the array (if the buffer was
                                not large enough).
  @param[out]      Buffer       The buffer in which the array is returned.
  @retval EFI_SUCCESS           The array of handles was returned.
  @retval EFI_NOT_FOUND         No handles match the search.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small for the result.
  @retval EFI_INVALID_PARAMETER SearchType is not a member of EFI_LOCATE_SEARCH_TYPE.
  @retval EFI_INVALID_PARAMETER SearchType is ByRegisterNotify and SearchKey is NULL.
  @retval EFI_INVALID_PARAMETER SearchType is ByProtocol and Protocol is NULL.
  @retval EFI_INVALID_PARAMETER One or more matches are found and BufferSize is NULL.
  @retval EFI_INVALID_PARAMETER BufferSize is large enough for the result and Buffer is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_HANDLE)(
    IN     EFI_LOCATE_SEARCH_TYPE   SearchType,
    IN     EFI_GUID          const  *Protocol,    OPTIONAL
    IN     VOID                     *SearchKey,   OPTIONAL
    IN OUT UINTN                    *BufferSize,
    OUT    EFI_HANDLE               *Buffer
    );

/**
  This protocol can be used on any device handle to obtain generic path/location
  information concerning the physical device or logical device. If the handle does
  not logically map to a physical device, the handle may not necessarily support
  the device path protocol. The device path describes the location of the device
  the handle is for. The size of the Device Path can be determined from the structures
  that make up the Device Path.
**/
struct EFI_DEVICE_PATH_PROTOCOL {
    UINT8 Type;       ///< 0x01 Hardware Device Path.
                    ///< 0x02 ACPI Device Path.
                    ///< 0x03 Messaging Device Path.
                    ///< 0x04 Media Device Path.
                    ///< 0x05 BIOS Boot Specification Device Path.
                    ///< 0x7F End of Hardware Device Path.

    UINT8 SubType;    ///< Varies by Type
                    ///< 0xFF End Entire Device Path, or
                    ///< 0x01 End This Instance of a Device Path and start a new
                    ///< Device Path.

    UINT8 Length[2];  ///< Specific Device Path data. Type and Sub-Type define
                    ///< type of data. Size of data is included in Length.
};

/**
  Locates the handle to a device on the device path that supports the specified protocol.
  @param[in]       Protocol     Specifies the protocol to search for.
  @param[in, out]  DevicePath   On input, a pointer to a pointer to the device path. On output, the device
                                path pointer is modified to point to the remaining part of the device
                                path.
  @param[out]      Device       A pointer to the returned device handle.
  @retval EFI_SUCCESS           The resulting handle was returned.
  @retval EFI_NOT_FOUND         No handles match the search.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER DevicePath is NULL.
  @retval EFI_INVALID_PARAMETER A handle matched the search and Device is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_DEVICE_PATH)(
    IN     EFI_GUID                 const   *Protocol,
    IN OUT EFI_DEVICE_PATH_PROTOCOL         **DevicePath,
    OUT    EFI_HANDLE                       *Device
    );

/**
  Adds, updates, or removes a configuration table entry from the EFI System Table.
  @param[in]  Guid              A pointer to the GUID for the entry to add, update, or remove.
  @param[in]  Table             A pointer to the configuration table for the entry to add, update, or
                                remove. May be NULL.
  @retval EFI_SUCCESS           The (Guid, Table) pair was added, updated, or removed.
  @retval EFI_NOT_FOUND         An attempt was made to delete a nonexistent entry.
  @retval EFI_INVALID_PARAMETER Guid is NULL.
  @retval EFI_OUT_OF_RESOURCES  There is not enough memory available to complete the operation.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE)(
    IN EFI_GUID          const  *Guid,
    IN VOID                     *Table
    );

/**
  Loads an EFI image into memory.
  @param[in]   BootPolicy        If TRUE, indicates that the request originates from the boot
                                 manager, and that the boot manager is attempting to load
                                 FilePath as a boot selection. Ignored if SourceBuffer is
                                 not NULL.
  @param[in]   ParentImageHandle The caller's image handle.
  @param[in]   DevicePath        The DeviceHandle specific file path from which the image is
                                 loaded.
  @param[in]   SourceBuffer      If not NULL, a pointer to the memory location containing a copy
                                 of the image to be loaded.
  @param[in]   SourceSize        The size in bytes of SourceBuffer. Ignored if SourceBuffer is NULL.
  @param[out]  ImageHandle       The pointer to the returned image handle that is created when the
                                 image is successfully loaded.
  @retval EFI_SUCCESS            Image was loaded into memory correctly.
  @retval EFI_NOT_FOUND          Both SourceBuffer and DevicePath are NULL.
  @retval EFI_INVALID_PARAMETER  One or more parametes are invalid.
  @retval EFI_UNSUPPORTED        The image type is not supported.
  @retval EFI_OUT_OF_RESOURCES   Image was not loaded due to insufficient resources.
  @retval EFI_LOAD_ERROR         Image was not loaded because the image format was corrupt or not
                                 understood.
  @retval EFI_DEVICE_ERROR       Image was not loaded because the device returned a read error.
  @retval EFI_ACCESS_DENIED      Image was not loaded because the platform policy prohibits the
                                 image from being loaded. NULL is returned in *ImageHandle.
  @retval EFI_SECURITY_VIOLATION Image was loaded and an ImageHandle was created with a
                                 valid EFI_LOADED_IMAGE_PROTOCOL. However, the current
                                 platform policy specifies that the image should not be started.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_LOAD)(
    IN  BOOLEAN                      BootPolicy,
    IN  EFI_HANDLE                   ParentImageHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL     *DevicePath,
    IN  VOID                         *SourceBuffer OPTIONAL,
    IN  UINTN                        SourceSize,
    OUT EFI_HANDLE                   *ImageHandle
    );

/**
  Transfers control to a loaded image's entry point.
  @param[in]   ImageHandle       Handle of image to be started.
  @param[out]  ExitDataSize      The pointer to the size, in bytes, of ExitData.
  @param[out]  ExitData          The pointer to a pointer to a data buffer that includes a Null-terminated
                                 string, optionally followed by additional binary data.
  @retval EFI_INVALID_PARAMETER  ImageHandle is either an invalid image handle or the image
                                 has already been initialized with StartImage.
  @retval EFI_SECURITY_VIOLATION The current platform policy specifies that the image should not be started.
  @return Exit code from image
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_START)(
    IN  EFI_HANDLE                  ImageHandle,
    OUT UINTN                       *ExitDataSize,
    OUT CHAR16                      **ExitData    OPTIONAL
    );

/**
  Terminates a loaded EFI image and returns control to boot services.
  @param[in]  ImageHandle       Handle that identifies the image. This parameter is passed to the
                                image on entry.
  @param[in]  ExitStatus        The image's exit code.
  @param[in]  ExitDataSize      The size, in bytes, of ExitData. Ignored if ExitStatus is EFI_SUCCESS.
  @param[in]  ExitData          The pointer to a data buffer that includes a Null-terminated string,
                                optionally followed by additional binary data. The string is a
                                description that the caller may use to further indicate the reason
                                for the image's exit. ExitData is only valid if ExitStatus
                                is something other than EFI_SUCCESS. The ExitData buffer
                                must be allocated by calling AllocatePool().
  @retval EFI_SUCCESS           The image specified by ImageHandle was unloaded.
  @retval EFI_INVALID_PARAMETER The image specified by ImageHandle has been loaded and
                                started with LoadImage() and StartImage(), but the
                                image is not the currently executing image.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_EXIT)(
    IN  EFI_HANDLE                   ImageHandle,
    IN  EFI_STATUS                   ExitStatus,
    IN  UINTN                        ExitDataSize,
    IN  CHAR16                       *ExitData     OPTIONAL
    );

/**
  Unloads an image.
  @param[in]  ImageHandle       Handle that identifies the image to be unloaded.
  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_UNLOAD)(
    IN  EFI_HANDLE                   ImageHandle
    );

/**
  Terminates all boot services.
  @param[in]  ImageHandle       Handle that identifies the exiting image.
  @param[in]  MapKey            Key to the latest memory map.
  @retval EFI_SUCCESS           Boot services have been terminated.
  @retval EFI_INVALID_PARAMETER MapKey is incorrect.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    IN  EFI_HANDLE                   ImageHandle,
    IN  UINTN                        MapKey
    );

/**
  Returns a monotonically increasing count for the platform.
  @param[out]  Count            The pointer to returned value.
  @retval EFI_SUCCESS           The next monotonic count was returned.
  @retval EFI_INVALID_PARAMETER Count is NULL.
  @retval EFI_DEVICE_ERROR      The device is not functioning properly.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT)(
    OUT UINT64                  *Count
    );

/**
  Induces a fine-grained stall.
  @param[in]  Microseconds      The number of microseconds to stall execution.
  @retval EFI_SUCCESS           Execution was stalled at least the requested number of
                                Microseconds.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_STALL)(
    IN  UINTN                    Microseconds
    );

/**
  Sets the system's watchdog timer.
  @param[in]  Timeout           The number of seconds to set the watchdog timer to.
  @param[in]  WatchdogCode      The numeric code to log on a watchdog timer timeout event.
  @param[in]  DataSize          The size, in bytes, of WatchdogData.
  @param[in]  WatchdogData      A data buffer that includes a Null-terminated string, optionally
                                followed by additional binary data.
  @retval EFI_SUCCESS           The timeout has been set.
  @retval EFI_INVALID_PARAMETER The supplied WatchdogCode is invalid.
  @retval EFI_UNSUPPORTED       The system does not have a watchdog timer.
  @retval EFI_DEVICE_ERROR      The watchdog timer could not be programmed due to a hardware
                                error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SET_WATCHDOG_TIMER)(
    IN UINTN                    Timeout,
    IN UINT64                   WatchdogCode,
    IN UINTN                    DataSize,
    IN CHAR16                   *WatchdogData OPTIONAL
    );

/**
  Connects one or more drivers to a controller.
  @param[in]  ControllerHandle      The handle of the controller to which driver(s) are to be connected.
  @param[in]  DriverImageHandle     A pointer to an ordered list handles that support the
                                    EFI_DRIVER_BINDING_PROTOCOL.
  @param[in]  RemainingDevicePath   A pointer to the device path that specifies a child of the
                                    controller specified by ControllerHandle.
  @param[in]  Recursive             If TRUE, then ConnectController() is called recursively
                                    until the entire tree of controllers below the controller specified
                                    by ControllerHandle have been created. If FALSE, then
                                    the tree of controllers is only expanded one level.
  @retval EFI_SUCCESS           1) One or more drivers were connected to ControllerHandle.
                                2) No drivers were connected to ControllerHandle, but
                                RemainingDevicePath is not NULL, and it is an End Device
                                Path Node.
  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.
  @retval EFI_NOT_FOUND         1) There are no EFI_DRIVER_BINDING_PROTOCOL instances
                                present in the system.
                                2) No drivers were connected to ControllerHandle.
  @retval EFI_SECURITY_VIOLATION
                                The user has no permission to start UEFI device drivers on the device path
                                associated with the ControllerHandle or specified by the RemainingDevicePath.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CONNECT_CONTROLLER)(
    IN  EFI_HANDLE                    ControllerHandle,
    IN  EFI_HANDLE                    *DriverImageHandle,   OPTIONAL
    IN  EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath, OPTIONAL
    IN  BOOLEAN                       Recursive
    );

/**
  Disconnects one or more drivers from a controller.
  @param[in]  ControllerHandle      The handle of the controller from which driver(s) are to be disconnected.
  @param[in]  DriverImageHandle     The driver to disconnect from ControllerHandle.
                                    If DriverImageHandle is NULL, then all the drivers currently managing
                                    ControllerHandle are disconnected from ControllerHandle.
  @param[in]  ChildHandle           The handle of the child to destroy.
                                    If ChildHandle is NULL, then all the children of ControllerHandle are
                                    destroyed before the drivers are disconnected from ControllerHandle.
  @retval EFI_SUCCESS           1) One or more drivers were disconnected from the controller.
                                2) On entry, no drivers are managing ControllerHandle.
                                3) DriverImageHandle is not NULL, and on entry
                                   DriverImageHandle is not managing ControllerHandle.
  @retval EFI_INVALID_PARAMETER 1) ControllerHandle is NULL.
                                2) DriverImageHandle is not NULL, and it is not a valid EFI_HANDLE.
                                3) ChildHandle is not NULL, and it is not a valid EFI_HANDLE.
                                4) DriverImageHandle does not support the EFI_DRIVER_BINDING_PROTOCOL.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources available to disconnect any drivers from
                                ControllerHandle.
  @retval EFI_DEVICE_ERROR      The controller could not be disconnected because of a device error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DISCONNECT_CONTROLLER)(
    IN  EFI_HANDLE                     ControllerHandle,
    IN  EFI_HANDLE                     DriverImageHandle, OPTIONAL
    IN  EFI_HANDLE                     ChildHandle        OPTIONAL
    );

/**
  Queries a handle to determine if it supports a specified protocol. If the protocol is supported by the
  handle, it opens the protocol on behalf of the calling agent.
  @param[in]   Handle           The handle for the protocol interface that is being opened.
  @param[in]   Protocol         The published unique identifier of the protocol.
  @param[out]  Interface        Supplies the address where a pointer to the corresponding Protocol
                                Interface is returned.
  @param[in]   AgentHandle      The handle of the agent that is opening the protocol interface
                                specified by Protocol and Interface.
  @param[in]   ControllerHandle If the agent that is opening a protocol is a driver that follows the
                                UEFI Driver Model, then this parameter is the controller handle
                                that requires the protocol interface. If the agent does not follow
                                the UEFI Driver Model, then this parameter is optional and may
                                be NULL.
  @param[in]   Attributes       The open mode of the protocol interface specified by Handle
                                and Protocol.
  @retval EFI_SUCCESS           An item was added to the open list for the protocol interface, and the
                                protocol interface was returned in Interface.
  @retval EFI_UNSUPPORTED       Handle does not support Protocol.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_ACCESS_DENIED     Required attributes can't be supported in current environment.
  @retval EFI_ALREADY_STARTED   Item on the open list already has requierd attributes whose agent
                                handle is the same as AgentHandle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_OPEN_PROTOCOL)(
    IN  EFI_HANDLE                Handle,
    IN  EFI_GUID           const  *Protocol,
    OUT VOID                      **Interface, OPTIONAL
    IN  EFI_HANDLE                AgentHandle,
    IN  EFI_HANDLE                ControllerHandle,
    IN  UINT32                    Attributes
    );

/**
  Closes a protocol on a handle that was opened using OpenProtocol().
  @param[in]  Handle            The handle for the protocol interface that was previously opened
                                with OpenProtocol(), and is now being closed.
  @param[in]  Protocol          The published unique identifier of the protocol.
  @param[in]  AgentHandle       The handle of the agent that is closing the protocol interface.
  @param[in]  ControllerHandle  If the agent that opened a protocol is a driver that follows the
                                UEFI Driver Model, then this parameter is the controller handle
                                that required the protocol interface.
  @retval EFI_SUCCESS           The protocol instance was closed.
  @retval EFI_INVALID_PARAMETER 1) Handle is NULL.
                                2) AgentHandle is NULL.
                                3) ControllerHandle is not NULL and ControllerHandle is not a valid EFI_HANDLE.
                                4) Protocol is NULL.
  @retval EFI_NOT_FOUND         1) Handle does not support the protocol specified by Protocol.
                                2) The protocol interface specified by Handle and Protocol is not
                                   currently open by AgentHandle and ControllerHandle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CLOSE_PROTOCOL)(
    IN EFI_HANDLE               Handle,
    IN EFI_GUID         const   *Protocol,
    IN EFI_HANDLE               AgentHandle,
    IN EFI_HANDLE               ControllerHandle
    );

struct EFI_OPEN_PROTOCOL_INFORMATION_ENTRY {
    EFI_HANDLE  AgentHandle;
    EFI_HANDLE  ControllerHandle;
    UINT32      Attributes;
    UINT32      OpenCount;
};

/**
  Retrieves the list of agents that currently have a protocol interface opened.
  @param[in]   Handle           The handle for the protocol interface that is being queried.
  @param[in]   Protocol         The published unique identifier of the protocol.
  @param[out]  EntryBuffer      A pointer to a buffer of open protocol information in the form of
                                EFI_OPEN_PROTOCOL_INFORMATION_ENTRY structures.
  @param[out]  EntryCount       A pointer to the number of entries in EntryBuffer.
  @retval EFI_SUCCESS           The open protocol information was returned in EntryBuffer, and the
                                number of entries was returned EntryCount.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources available to allocate EntryBuffer.
  @retval EFI_NOT_FOUND         Handle does not support the protocol specified by Protocol.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION)(
    IN  EFI_HANDLE                          Handle,
    IN  EFI_GUID                  const     *Protocol,
    OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    OUT UINTN                               *EntryCount
    );

/**
  Retrieves the list of protocol interface GUIDs that are installed on a handle in a buffer allocated
  from pool.
  @param[in]   Handle              The handle from which to retrieve the list of protocol interface
                                   GUIDs.
  @param[out]  ProtocolBuffer      A pointer to the list of protocol interface GUID pointers that are
                                   installed on Handle.
  @param[out]  ProtocolBufferCount A pointer to the number of GUID pointers present in
                                   ProtocolBuffer.
  @retval EFI_SUCCESS           The list of protocol interface GUIDs installed on Handle was returned in
                                ProtocolBuffer. The number of protocol interface GUIDs was
                                returned in ProtocolBufferCount.
  @retval EFI_OUT_OF_RESOURCES  There is not enough pool memory to store the results.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Handle is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER ProtocolBuffer is NULL.
  @retval EFI_INVALID_PARAMETER ProtocolBufferCount is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PROTOCOLS_PER_HANDLE)(
    IN  EFI_HANDLE      Handle,
    OUT EFI_GUID const  ***ProtocolBuffer,
    OUT UINTN           *ProtocolBufferCount
    );

/**
  Returns an array of handles that support the requested protocol in a buffer allocated from pool.
  @param[in]       SearchType   Specifies which handle(s) are to be returned.
  @param[in]       Protocol     Provides the protocol to search by.
                                This parameter is only valid for a SearchType of ByProtocol.
  @param[in]       SearchKey    Supplies the search key depending on the SearchType.
  @param[in, out]  NoHandles    The number of handles returned in Buffer.
  @param[out]      Buffer       A pointer to the buffer to return the requested array of handles that
                                support Protocol.
  @retval EFI_SUCCESS           The array of handles was returned in Buffer, and the number of
                                handles in Buffer was returned in NoHandles.
  @retval EFI_NOT_FOUND         No handles match the search.
  @retval EFI_OUT_OF_RESOURCES  There is not enough pool memory to store the matching results.
  @retval EFI_INVALID_PARAMETER NoHandles is NULL.
  @retval EFI_INVALID_PARAMETER Buffer is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    IN     EFI_LOCATE_SEARCH_TYPE       SearchType,
    IN     EFI_GUID              const  *Protocol,      OPTIONAL
    IN     VOID                         *SearchKey,     OPTIONAL
    IN OUT UINTN                        *NoHandles,
    OUT    EFI_HANDLE                   **Buffer
    );

/**
  Returns the first protocol instance that matches the given protocol.
  @param[in]  Protocol          Provides the protocol to search for.
  @param[in]  Registration      Optional registration key returned from
                                RegisterProtocolNotify().
  @param[out]  Interface        On return, a pointer to the first interface that matches Protocol and
                                Registration.
  @retval EFI_SUCCESS           A protocol instance matching Protocol was found and returned in
                                Interface.
  @retval EFI_NOT_FOUND         No protocol instances were found that match Protocol and
                                Registration.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_PROTOCOL)(
    IN  EFI_GUID const *Protocol,
    IN  VOID      *Registration, OPTIONAL
    OUT VOID      **Interface
    );

/**
  Installs one or more protocol interfaces into the boot services environment.
  @param[in, out]  Handle       The pointer to a handle to install the new protocol interfaces on,
                                or a pointer to NULL if a new handle is to be allocated.
  @param  ...                   A variable argument list containing pairs of protocol GUIDs and protocol
                                interfaces.
  @retval EFI_SUCCESS           All the protocol interface was installed.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.
  @retval EFI_ALREADY_STARTED   A Device Path Protocol instance was passed in that is already present in
                                the handle database.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is already installed on the handle specified by Handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    IN OUT EFI_HANDLE           *Handle,
    ...
    );

/**
  Removes one or more protocol interfaces into the boot services environment.
  @param[in]  Handle            The handle to remove the protocol interfaces from.
  @param  ...                   A variable argument list containing pairs of protocol GUIDs and
                                protocol interfaces.
  @retval EFI_SUCCESS           All the protocol interfaces were removed.
  @retval EFI_INVALID_PARAMETER One of the protocol interfaces was not previously installed on Handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    IN EFI_HANDLE           Handle,
    ...
    );

/**
  Computes and returns a 32-bit CRC for a data buffer.
  @param[in]   Data             A pointer to the buffer on which the 32-bit CRC is to be computed.
  @param[in]   DataSize         The number of bytes in the buffer Data.
  @param[out]  Crc32            The 32-bit CRC that was computed for the data buffer specified by Data
                                and DataSize.
  @retval EFI_SUCCESS           The 32-bit CRC was computed for the data buffer and returned in
                                Crc32.
  @retval EFI_INVALID_PARAMETER Data is NULL.
  @retval EFI_INVALID_PARAMETER Crc32 is NULL.
  @retval EFI_INVALID_PARAMETER DataSize is 0.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CALCULATE_CRC32)(
    IN  VOID                              *Data,
    IN  UINTN                             DataSize,
    OUT UINT32                            *Crc32
    );

/**
  Copies the contents of one buffer to another buffer.
  @param[in]  Destination       The pointer to the destination buffer of the memory copy.
  @param[in]  Source            The pointer to the source buffer of the memory copy.
  @param[in]  Length            Number of bytes to copy from Source to Destination.
**/
typedef
VOID
(EFIAPI *EFI_COPY_MEM)(
    IN VOID     *Destination,
    IN VOID     *Source,
    IN UINTN    Length
    );

/**
  The SetMem() function fills a buffer with a specified value.
  @param[in]  Buffer            The pointer to the buffer to fill.
  @param[in]  Size              Number of bytes in Buffer to fill.
  @param[in]  Value             Value to fill Buffer with.
**/
typedef
VOID
(EFIAPI *EFI_SET_MEM)(
    IN VOID     *Buffer,
    IN UINTN    Size,
    IN UINT8    Value
    );

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    //
    // Task Priority Services
    //
    EFI_RAISE_TPL RaiseTPL;
    EFI_RESTORE_TPL RestoreTPL;

    //
    // Memory Services
    //
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;

    //
    // Event & Timer Services
    //
    EFI_CREATE_EVENT CreateEvent;
    EFI_SET_TIMER SetTimer;
    EFI_WAIT_FOR_EVENT WaitForEvent;
    EFI_SIGNAL_EVENT SignalEvent;
    EFI_CLOSE_EVENT CloseEvent;
    EFI_CHECK_EVENT CheckEvent;

    //
    // Protocol Handler Services
    //
    EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;
    EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    VOID *Reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;
    EFI_LOCATE_HANDLE LocateHandle;
    EFI_LOCATE_DEVICE_PATH LocateDevicePath;
    EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;

    //
    // Image Services
    //
    EFI_IMAGE_LOAD LoadImage;
    EFI_IMAGE_START StartImage;
    EFI_EXIT Exit;
    EFI_IMAGE_UNLOAD UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;

    //
    // Miscellaneous Services
    //
    EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount;
    EFI_STALL Stall;
    EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;

    //
    // DriverSupport Services
    //
    EFI_CONNECT_CONTROLLER ConnectController;
    EFI_DISCONNECT_CONTROLLER DisconnectController;

    //
    // Open and Close Protocol Services
    //
    EFI_OPEN_PROTOCOL OpenProtocol;
    EFI_CLOSE_PROTOCOL CloseProtocol;
    EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation;

    //
    // Library Services
    //
    EFI_PROTOCOLS_PER_HANDLE ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;

    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES InstallMultipleProtocolInterfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
            UninstallMultipleProtocolInterfaces;

    //
    // 32-bit CRC Services
    //
    EFI_CALCULATE_CRC32 CalculateCrc32;

    //
    // Memory Utility Services
    //
    EFI_COPY_MEM CopyMem;
    EFI_SET_MEM SetMem;
};

struct EFI_CONFIGURATION_TABLE {
    EFI_GUID VendorGuid;
    VOID *VendorTable;
};

///
/// Protocol GUID defined in EFI1.1.
///
#define SIMPLE_TEXT_OUTPUT_PROTOCOL   EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID

///
/// Backward-compatible with EFI1.1.
///

//
// Define's for required EFI Unicode Box Draw characters
//
#define BOXDRAW_HORIZONTAL                  0x2500
#define BOXDRAW_VERTICAL                    0x2502
#define BOXDRAW_DOWN_RIGHT                  0x250c
#define BOXDRAW_DOWN_LEFT                   0x2510
#define BOXDRAW_UP_RIGHT                    0x2514
#define BOXDRAW_UP_LEFT                     0x2518
#define BOXDRAW_VERTICAL_RIGHT              0x251c
#define BOXDRAW_VERTICAL_LEFT               0x2524
#define BOXDRAW_DOWN_HORIZONTAL             0x252c
#define BOXDRAW_UP_HORIZONTAL               0x2534
#define BOXDRAW_VERTICAL_HORIZONTAL         0x253c
#define BOXDRAW_DOUBLE_HORIZONTAL           0x2550
#define BOXDRAW_DOUBLE_VERTICAL             0x2551
#define BOXDRAW_DOWN_RIGHT_DOUBLE           0x2552
#define BOXDRAW_DOWN_DOUBLE_RIGHT           0x2553
#define BOXDRAW_DOUBLE_DOWN_RIGHT           0x2554
#define BOXDRAW_DOWN_LEFT_DOUBLE            0x2555
#define BOXDRAW_DOWN_DOUBLE_LEFT            0x2556
#define BOXDRAW_DOUBLE_DOWN_LEFT            0x2557
#define BOXDRAW_UP_RIGHT_DOUBLE             0x2558
#define BOXDRAW_UP_DOUBLE_RIGHT             0x2559
#define BOXDRAW_DOUBLE_UP_RIGHT             0x255a
#define BOXDRAW_UP_LEFT_DOUBLE              0x255b
#define BOXDRAW_UP_DOUBLE_LEFT              0x255c
#define BOXDRAW_DOUBLE_UP_LEFT              0x255d
#define BOXDRAW_VERTICAL_RIGHT_DOUBLE       0x255e
#define BOXDRAW_VERTICAL_DOUBLE_RIGHT       0x255f
#define BOXDRAW_DOUBLE_VERTICAL_RIGHT       0x2560
#define BOXDRAW_VERTICAL_LEFT_DOUBLE        0x2561
#define BOXDRAW_VERTICAL_DOUBLE_LEFT        0x2562
#define BOXDRAW_DOUBLE_VERTICAL_LEFT        0x2563
#define BOXDRAW_DOWN_HORIZONTAL_DOUBLE      0x2564
#define BOXDRAW_DOWN_DOUBLE_HORIZONTAL      0x2565
#define BOXDRAW_DOUBLE_DOWN_HORIZONTAL      0x2566
#define BOXDRAW_UP_HORIZONTAL_DOUBLE        0x2567
#define BOXDRAW_UP_DOUBLE_HORIZONTAL        0x2568
#define BOXDRAW_DOUBLE_UP_HORIZONTAL        0x2569
#define BOXDRAW_VERTICAL_HORIZONTAL_DOUBLE  0x256a
#define BOXDRAW_VERTICAL_DOUBLE_HORIZONTAL  0x256b
#define BOXDRAW_DOUBLE_VERTICAL_HORIZONTAL  0x256c

//
// EFI Required Block Elements Code Chart
//
#define BLOCKELEMENT_FULL_BLOCK   0x2588
#define BLOCKELEMENT_LIGHT_SHADE  0x2591

//
// EFI Required Geometric Shapes Code Chart
//
#define GEOMETRICSHAPE_UP_TRIANGLE    0x25b2
#define GEOMETRICSHAPE_RIGHT_TRIANGLE 0x25ba
#define GEOMETRICSHAPE_DOWN_TRIANGLE  0x25bc
#define GEOMETRICSHAPE_LEFT_TRIANGLE  0x25c4

//
// EFI Required Arrow shapes
//
#define ARROW_LEFT  0x2190
#define ARROW_UP    0x2191
#define ARROW_RIGHT 0x2192
#define ARROW_DOWN  0x2193

//
// EFI Console Colours
//
#define EFI_BLACK                 0x00
#define EFI_BLUE                  0x01
#define EFI_GREEN                 0x02
#define EFI_CYAN                  (EFI_BLUE | EFI_GREEN)
#define EFI_RED                   0x04
#define EFI_MAGENTA               (EFI_BLUE | EFI_RED)
#define EFI_BROWN                 (EFI_GREEN | EFI_RED)
#define EFI_LIGHTGRAY             (EFI_BLUE | EFI_GREEN | EFI_RED)
#define EFI_BRIGHT                0x08
#define EFI_DARKGRAY              (EFI_BLACK | EFI_BRIGHT)
#define EFI_LIGHTBLUE             (EFI_BLUE | EFI_BRIGHT)
#define EFI_LIGHTGREEN            (EFI_GREEN | EFI_BRIGHT)
#define EFI_LIGHTCYAN             (EFI_CYAN | EFI_BRIGHT)
#define EFI_LIGHTRED              (EFI_RED | EFI_BRIGHT)
#define EFI_LIGHTMAGENTA          (EFI_MAGENTA | EFI_BRIGHT)
#define EFI_YELLOW                (EFI_BROWN | EFI_BRIGHT)
#define EFI_WHITE                 (EFI_BLUE | EFI_GREEN | EFI_RED | EFI_BRIGHT)

//
// Macro to accept color values in their raw form to create
// a value that represents both a foreground and background
// color in a single byte.
// For Foreground, and EFI_* value is valid from EFI_BLACK(0x00) to
// EFI_WHITE (0x0F).
// For Background, only EFI_BLACK, EFI_BLUE, EFI_GREEN, EFI_CYAN,
// EFI_RED, EFI_MAGENTA, EFI_BROWN, and EFI_LIGHTGRAY are acceptable
//
// Do not use EFI_BACKGROUND_xxx values with this macro.
//
#define EFI_TEXT_ATTR(Foreground,Background) ((Foreground) | ((Background) << 4))

#define EFI_BACKGROUND_BLACK      0x00
#define EFI_BACKGROUND_BLUE       0x10
#define EFI_BACKGROUND_GREEN      0x20
#define EFI_BACKGROUND_CYAN       (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_GREEN)
#define EFI_BACKGROUND_RED        0x40
#define EFI_BACKGROUND_MAGENTA    (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_RED)
#define EFI_BACKGROUND_BROWN      (EFI_BACKGROUND_GREEN | EFI_BACKGROUND_RED)
#define EFI_BACKGROUND_LIGHTGRAY  (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_GREEN | EFI_BACKGROUND_RED)

//
// We currently define attributes from 0 - 7F for color manipulations
// To internally handle the local display characteristics for a particular character,
// Bit 7 signifies the local glyph representation for a character.  If turned on, glyphs will be
// pulled from the wide glyph database and will display locally as a wide character (16 X 19 versus 8 X 19)
// If bit 7 is off, the narrow glyph database will be used.  This does NOT affect information that is sent to
// non-local displays, such as serial or LAN consoles.
//
#define EFI_WIDE_ATTRIBUTE  0x80

/**
  Reset the text output device hardware and optionaly run diagnostics
  @param  This                 The protocol instance pointer.
  @param  ExtendedVerification Driver may perform more exhaustive verfication
                               operation of the device during reset.
  @retval EFI_SUCCESS          The text output device was reset.
  @retval EFI_DEVICE_ERROR     The text output device is not functioning correctly and
                               could not be reset.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_RESET)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN BOOLEAN                                ExtendedVerification
    );

/**
  Write a string to the output device.
  @param  This   The protocol instance pointer.
  @param  String The NULL-terminated string to be displayed on the output
                 device(s). All output devices must also support the Unicode
                 drawing character codes defined in this file.
  @retval EFI_SUCCESS             The string was output to the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting to output
                                  the text.
  @retval EFI_UNSUPPORTED         The output device's mode is not currently in a
                                  defined text mode.
  @retval EFI_WARN_UNKNOWN_GLYPH  This warning code indicates that some of the
                                  characters in the string could not be
                                  rendered and were skipped.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN CHAR16                         const   *String   // <-- DGOS added const
    );

/**
  Verifies that all characters in a string can be output to the
  target device.
  @param  This   The protocol instance pointer.
  @param  String The NULL-terminated string to be examined for the output
                 device(s).
  @retval EFI_SUCCESS      The device(s) are capable of rendering the output string.
  @retval EFI_UNSUPPORTED  Some of the characters in the string cannot be
                           rendered by one or more of the output devices mapped
                           by the EFI handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_TEST_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN CHAR16                                 *String
    );

/**
  Returns information for an available text mode that the output device(s)
  supports.
  @param  This       The protocol instance pointer.
  @param  ModeNumber The mode number to return information on.
  @param  Columns    Returns the geometry of the text output device for the
                     requested ModeNumber.
  @param  Rows       Returns the geometry of the text output device for the
                     requested ModeNumber.

  @retval EFI_SUCCESS      The requested mode information was returned.
  @retval EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED  The mode number was not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_QUERY_MODE)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN UINTN                                  ModeNumber,
    OUT UINTN                                 *Columns,
    OUT UINTN                                 *Rows
    );

/**
  Sets the output device(s) to a specified mode.
  @param  This       The protocol instance pointer.
  @param  ModeNumber The mode number to set.
  @retval EFI_SUCCESS      The requested text mode was set.
  @retval EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED  The mode number was not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_MODE)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN UINTN                                  ModeNumber
    );

/**
  Sets the background and foreground colors for the OutputString () and
  ClearScreen () functions.
  @param  This      The protocol instance pointer.
  @param  Attribute The attribute to set. Bits 0..3 are the foreground color, and
                    bits 4..6 are the background color. All other bits are undefined
                    and must be zero. The valid Attributes are defined in this file.
  @retval EFI_SUCCESS       The attribute was set.
  @retval EFI_DEVICE_ERROR  The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED   The attribute requested is not defined.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN UINTN                                  Attribute
    );

/**
  Clears the output device(s) display to the currently selected background
  color.
  @param  This              The protocol instance pointer.

  @retval  EFI_SUCCESS      The operation completed successfully.
  @retval  EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval  EFI_UNSUPPORTED  The output device is not in a valid text mode.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *This
    );

/**
  Sets the current coordinates of the cursor position
  @param  This        The protocol instance pointer.
  @param  Column      The position to set the cursor to. Must be greater than or
                      equal to zero and less than the number of columns and rows
                      by QueryMode ().
  @param  Row         The position to set the cursor to. Must be greater than or
                      equal to zero and less than the number of columns and rows
                      by QueryMode ().
  @retval EFI_SUCCESS      The operation completed successfully.
  @retval EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED  The output device is not in a valid text mode, or the
                           cursor position is invalid for the current mode.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN UINTN                                  Column,
    IN UINTN                                  Row
    );

/**
  Makes the cursor visible or invisible
  @param  This    The protocol instance pointer.
  @param  Visible If TRUE, the cursor is set to be visible. If FALSE, the cursor is
                  set to be invisible.
  @retval EFI_SUCCESS      The operation completed successfully.
  @retval EFI_DEVICE_ERROR The device had an error and could not complete the
                           request, or the device does not support changing
                           the cursor mode.
  @retval EFI_UNSUPPORTED  The output device is not in a valid text mode.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
    IN BOOLEAN                                Visible
    );

/**
  @par Data Structure Description:
  Mode Structure pointed to by Simple Text Out protocol.
**/
struct EFI_SIMPLE_TEXT_OUTPUT_MODE {
    ///
    /// The number of modes supported by QueryMode () and SetMode ().
    ///
    INT32   MaxMode;

    //
    // current settings
    //

    ///
    /// The text mode of the output device(s).
    ///
    INT32   Mode;
    ///
    /// The current character output attribute.
    ///
    INT32   Attribute;
    ///
    /// The cursor's column.
    ///
    INT32   CursorColumn;
    ///
    /// The cursor's row.
    ///
    INT32   CursorRow;
    ///
    /// The cursor is currently visbile or not.
    ///
    BOOLEAN CursorVisible;
};

///
/// The SIMPLE_TEXT_OUTPUT protocol is used to control text-based output devices.
/// It is the minimum required protocol for any handle supplied as the ConsoleOut
/// or StandardError device. In addition, the minimum supported text mode of such
/// devices is at least 80 x 25 characters.
///
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET                Reset;

    EFI_TEXT_STRING               OutputString;
    EFI_TEXT_TEST_STRING          TestString;

    EFI_TEXT_QUERY_MODE           QueryMode;
    EFI_TEXT_SET_MODE             SetMode;
    EFI_TEXT_SET_ATTRIBUTE        SetAttribute;

    EFI_TEXT_CLEAR_SCREEN         ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION  SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR        EnableCursor;

    ///
    /// Pointer to SIMPLE_TEXT_OUTPUT_MODE data.
    ///
    EFI_SIMPLE_TEXT_OUTPUT_MODE   *Mode;
};

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
  { \
    0x387477c1, 0x69c7, 0x11d2, { \
        0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b \
    } \
  }

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL  EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

///
/// Protocol GUID name defined in EFI1.1.
///
#define SIMPLE_INPUT_PROTOCOL   EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID

///
/// The keystroke information for the key that was pressed.
///
typedef struct {
    UINT16  ScanCode;
    CHAR16  UnicodeChar;
} EFI_INPUT_KEY;

//
// Required unicode control chars
//
#define EFI_CHAR_NULL             0x0000
#define EFI_CHAR_BACKSPACE        0x0008
#define EFI_CHAR_TAB              0x0009
#define EFI_CHAR_LINEFEED         0x000A
#define EFI_CHAR_CARRIAGE_RETURN  0x000D

//
// EFI Scan codes
//
#define EFI_SCAN_NULL       0x0000
#define EFI_SCAN_UP         0x0001
#define EFI_SCAN_DOWN       0x0002
#define EFI_SCAN_RIGHT      0x0003
#define EFI_SCAN_LEFT       0x0004
#define EFI_SCAN_HOME       0x0005
#define EFI_SCAN_END        0x0006
#define EFI_SCAN_INSERT     0x0007
#define EFI_SCAN_DELETE     0x0008
#define EFI_SCAN_PAGE_UP    0x0009
#define EFI_SCAN_PAGE_DOWN  0x000A
#define EFI_SCAN_F1         0x000B
#define EFI_SCAN_F2         0x000C
#define EFI_SCAN_F3         0x000D
#define EFI_SCAN_F4         0x000E
#define EFI_SCAN_F5         0x000F
#define EFI_SCAN_F6         0x0010
#define EFI_SCAN_F7         0x0011
#define EFI_SCAN_F8         0x0012
#define EFI_SCAN_F9         0x0013
#define EFI_SCAN_F10        0x0014
#define EFI_SCAN_ESC        0x0017

/**
  Reset the input device and optionally run diagnostics
  @param  This                 Protocol instance pointer.
  @param  ExtendedVerification Driver may perform diagnostics on reset.
  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could not be reset.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_RESET)(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL       *This,
    IN BOOLEAN                              ExtendedVerification
    );

/**
  Reads the next keystroke from the input device. The WaitForKey Event can
  be used to test for existence of a keystroke via WaitForEvent () call.
  @param  This  Protocol instance pointer.
  @param  Key   A pointer to a buffer that is filled in with the keystroke
                information for the key that was pressed.
  @retval EFI_SUCCESS      The keystroke information was returned.
  @retval EFI_NOT_READY    There was no keystroke data available.
  @retval EFI_DEVICE_ERROR The keystroke information was not returned due to
                           hardware errors.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_READ_KEY)(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL       *This,
    OUT EFI_INPUT_KEY                       *Key
    );

///
/// The EFI_SIMPLE_TEXT_INPUT_PROTOCOL is used on the ConsoleIn device.
/// It is the minimum required protocol for ConsoleIn.
///
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET     Reset;
    EFI_INPUT_READ_KEY  ReadKeyStroke;
    ///
    /// Event to use with WaitForEvent() to wait for a key to be available
    ///
    EFI_EVENT           WaitForKey;
};

#define ACPI_20_TABLE_GUID \
    {0x8868e871,0xe4f1,0x11d3,0xbc,0x22,0x0,0x80,0xc7,0x3c,0x88,0x81}
#define ACPI_TABLE_GUID \
    {0xeb9d2d30,0x2d88,0x11d3,0x9a,0x16,0x0,0x90,0x27,0x3f,0xc1,0x4d}
#define SAL_SYSTEM_TABLE_GUID \
    {0xeb9d2d32,0x2d88,0x11d3,0x9a,0x16,0x0,0x90,0x27,0x3f,0xc1,0x4d}
#define SMBIOS_TABLE_GUID \
    {0xeb9d2d31,0x2d88,0x11d3,0x9a,0x16,0x0,0x90,0x27,0x3f,0xc1,0x4d}
#define MPS_TABLE_GUID \
    {0xeb9d2d2f,0x2d88,0x11d3,0x9a,0x16,0x0,0x90,0x27,0x3f,0xc1,0x4d}


//
// Status codes common to all execution phases
//
typedef EFI_STATUS RETURN_STATUS;

#define MAX_BIT (~((UINTN)-1 >> 1))

/**
  Produces a RETURN_STATUS code with the highest bit set.
  @param  StatusCode    The status code value to convert into a warning code.
                        StatusCode must be in the range 0x00000000..0x7FFFFFFF.
  @return The value specified by StatusCode with the highest bit set.
**/
#define ENCODE_ERROR(StatusCode)     ((RETURN_STATUS)(MAX_BIT | (StatusCode)))

/**
  Produces a RETURN_STATUS code with the highest bit clear.
  @param  StatusCode    The status code value to convert into a warning code.
                        StatusCode must be in the range 0x00000000..0x7FFFFFFF.
  @return The value specified by StatusCode with the highest bit clear.
**/
#define ENCODE_WARNING(StatusCode)   ((RETURN_STATUS)(StatusCode))

/**
  Returns TRUE if a specified RETURN_STATUS code is an error code.
  This function returns TRUE if StatusCode has the high bit set.  Otherwise, FALSE is returned.
  @param  StatusCode    The status code value to evaluate.
  @retval TRUE          The high bit of StatusCode is set.
  @retval FALSE         The high bit of StatusCode is clear.
**/
#define RETURN_ERROR(StatusCode)     (((StatusCode)) < 0)

///
/// The operation completed successfully.
///
#define RETURN_SUCCESS               0

///
/// The image failed to load.
///
#define RETURN_LOAD_ERROR            ENCODE_ERROR (1)

///
/// The parameter was incorrect.
///
#define RETURN_INVALID_PARAMETER     ENCODE_ERROR (2)

///
/// The operation is not supported.
///
#define RETURN_UNSUPPORTED           ENCODE_ERROR (3)

///
/// The buffer was not the proper size for the request.
///
#define RETURN_BAD_BUFFER_SIZE       ENCODE_ERROR (4)

///
/// The buffer was not large enough to hold the requested data.
/// The required buffer size is returned in the appropriate
/// parameter when this error occurs.
///
#define RETURN_BUFFER_TOO_SMALL      ENCODE_ERROR (5)

///
/// There is no data pending upon return.
///
#define RETURN_NOT_READY             ENCODE_ERROR (6)

///
/// The physical device reported an error while attempting the
/// operation.
///
#define RETURN_DEVICE_ERROR          ENCODE_ERROR (7)

///
/// The device can not be written to.
///
#define RETURN_WRITE_PROTECTED       ENCODE_ERROR (8)

///
/// The resource has run out.
///
#define RETURN_OUT_OF_RESOURCES      ENCODE_ERROR (9)

///
/// An inconsistency was detected on the file system causing the
/// operation to fail.
///
#define RETURN_VOLUME_CORRUPTED      ENCODE_ERROR (10)

///
/// There is no more space on the file system.
///
#define RETURN_VOLUME_FULL           ENCODE_ERROR (11)

///
/// The device does not contain any medium to perform the
/// operation.
///
#define RETURN_NO_MEDIA              ENCODE_ERROR (12)

///
/// The medium in the device has changed since the last
/// access.
///
#define RETURN_MEDIA_CHANGED         ENCODE_ERROR (13)

///
/// The item was not found.
///
#define RETURN_NOT_FOUND             ENCODE_ERROR (14)

///
/// Access was denied.
///
#define RETURN_ACCESS_DENIED         ENCODE_ERROR (15)

///
/// The server was not found or did not respond to the request.
///
#define RETURN_NO_RESPONSE           ENCODE_ERROR (16)

///
/// A mapping to the device does not exist.
///
#define RETURN_NO_MAPPING            ENCODE_ERROR (17)

///
/// A timeout time expired.
///
#define RETURN_TIMEOUT               ENCODE_ERROR (18)

///
/// The protocol has not been started.
///
#define RETURN_NOT_STARTED           ENCODE_ERROR (19)

///
/// The protocol has already been started.
///
#define RETURN_ALREADY_STARTED       ENCODE_ERROR (20)

///
/// The operation was aborted.
///
#define RETURN_ABORTED               ENCODE_ERROR (21)

///
/// An ICMP error occurred during the network operation.
///
#define RETURN_ICMP_ERROR            ENCODE_ERROR (22)

///
/// A TFTP error occurred during the network operation.
///
#define RETURN_TFTP_ERROR            ENCODE_ERROR (23)

///
/// A protocol error occurred during the network operation.
///
#define RETURN_PROTOCOL_ERROR        ENCODE_ERROR (24)

///
/// A function encountered an internal version that was
/// incompatible with a version requested by the caller.
///
#define RETURN_INCOMPATIBLE_VERSION  ENCODE_ERROR (25)

///
/// The function was not performed due to a security violation.
///
#define RETURN_SECURITY_VIOLATION    ENCODE_ERROR (26)

///
/// A CRC error was detected.
///
#define RETURN_CRC_ERROR             ENCODE_ERROR (27)

///
/// The beginning or end of media was reached.
///
#define RETURN_END_OF_MEDIA          ENCODE_ERROR (28)

///
/// The end of the file was reached.
///
#define RETURN_END_OF_FILE           ENCODE_ERROR (31)

///
/// The language specified was invalid.
///
#define RETURN_INVALID_LANGUAGE      ENCODE_ERROR (32)

///
/// The security status of the data is unknown or compromised
/// and the data must be updated or replaced to restore a valid
/// security status.
///
#define RETURN_COMPROMISED_DATA      ENCODE_ERROR (33)

///
/// A HTTP error occurred during the network operation.
///
#define RETURN_HTTP_ERROR            ENCODE_ERROR (35)

///
/// The string contained one or more characters that
/// the device could not render and were skipped.
///
#define RETURN_WARN_UNKNOWN_GLYPH    ENCODE_WARNING (1)

///
/// The handle was closed, but the file was not deleted.
///
#define RETURN_WARN_DELETE_FAILURE   ENCODE_WARNING (2)

///
/// The handle was closed, but the data to the file was not
/// flushed properly.
///
#define RETURN_WARN_WRITE_FAILURE    ENCODE_WARNING (3)

///
/// The resulting buffer was too small, and the data was
/// truncated to the buffer size.
///
#define RETURN_WARN_BUFFER_TOO_SMALL ENCODE_WARNING (4)

///
/// The data has not been updated within the timeframe set by
/// local policy for this type of data.
///
#define RETURN_WARN_STALE_DATA       ENCODE_WARNING (5)

///
/// The resulting buffer contains UEFI-compliant file system.
///
#define RETURN_WARN_FILE_SYSTEM      ENCODE_WARNING (6)

///
/// Enumeration of EFI_STATUS.
///@{
#define EFI_SUCCESS               RETURN_SUCCESS
#define EFI_LOAD_ERROR            RETURN_LOAD_ERROR
#define EFI_INVALID_PARAMETER     RETURN_INVALID_PARAMETER
#define EFI_UNSUPPORTED           RETURN_UNSUPPORTED
#define EFI_BAD_BUFFER_SIZE       RETURN_BAD_BUFFER_SIZE
#define EFI_BUFFER_TOO_SMALL      RETURN_BUFFER_TOO_SMALL
#define EFI_NOT_READY             RETURN_NOT_READY
#define EFI_DEVICE_ERROR          RETURN_DEVICE_ERROR
#define EFI_WRITE_PROTECTED       RETURN_WRITE_PROTECTED
#define EFI_OUT_OF_RESOURCES      RETURN_OUT_OF_RESOURCES
#define EFI_VOLUME_CORRUPTED      RETURN_VOLUME_CORRUPTED
#define EFI_VOLUME_FULL           RETURN_VOLUME_FULL
#define EFI_NO_MEDIA              RETURN_NO_MEDIA
#define EFI_MEDIA_CHANGED         RETURN_MEDIA_CHANGED
#define EFI_NOT_FOUND             RETURN_NOT_FOUND
#define EFI_ACCESS_DENIED         RETURN_ACCESS_DENIED
#define EFI_NO_RESPONSE           RETURN_NO_RESPONSE
#define EFI_NO_MAPPING            RETURN_NO_MAPPING
#define EFI_TIMEOUT               RETURN_TIMEOUT
#define EFI_NOT_STARTED           RETURN_NOT_STARTED
#define EFI_ALREADY_STARTED       RETURN_ALREADY_STARTED
#define EFI_ABORTED               RETURN_ABORTED
#define EFI_ICMP_ERROR            RETURN_ICMP_ERROR
#define EFI_TFTP_ERROR            RETURN_TFTP_ERROR
#define EFI_PROTOCOL_ERROR        RETURN_PROTOCOL_ERROR
#define EFI_INCOMPATIBLE_VERSION  RETURN_INCOMPATIBLE_VERSION
#define EFI_SECURITY_VIOLATION    RETURN_SECURITY_VIOLATION
#define EFI_CRC_ERROR             RETURN_CRC_ERROR
#define EFI_END_OF_MEDIA          RETURN_END_OF_MEDIA
#define EFI_END_OF_FILE           RETURN_END_OF_FILE
#define EFI_INVALID_LANGUAGE      RETURN_INVALID_LANGUAGE
#define EFI_COMPROMISED_DATA      RETURN_COMPROMISED_DATA
#define EFI_HTTP_ERROR            RETURN_HTTP_ERROR

#define EFI_WARN_UNKNOWN_GLYPH    RETURN_WARN_UNKNOWN_GLYPH
#define EFI_WARN_DELETE_FAILURE   RETURN_WARN_DELETE_FAILURE
#define EFI_WARN_WRITE_FAILURE    RETURN_WARN_WRITE_FAILURE
#define EFI_WARN_BUFFER_TOO_SMALL RETURN_WARN_BUFFER_TOO_SMALL
#define EFI_WARN_STALE_DATA       RETURN_WARN_STALE_DATA
#define EFI_WARN_FILE_SYSTEM      RETURN_WARN_FILE_SYSTEM
///@}

///
/// Define macro to encode the status code.
///
#define EFIERR(_a)                ENCODE_ERROR(_a)

#define EFI_ERROR(A)              RETURN_ERROR(A)

///
/// ICMP error definitions
///@{
#define EFI_NETWORK_UNREACHABLE   EFIERR(100)
#define EFI_HOST_UNREACHABLE      EFIERR(101)
#define EFI_PROTOCOL_UNREACHABLE  EFIERR(102)
#define EFI_PORT_UNREACHABLE      EFIERR(103)
///@}

///
/// Tcp connection status definitions
///@{
#define EFI_CONNECTION_FIN        EFIERR(104)
#define EFI_CONNECTION_RESET      EFIERR(105)
#define EFI_CONNECTION_REFUSED    EFIERR(106)
///@}

struct EFI_LOADED_IMAGE_PROTOCOL {
    UINT32            Revision;       ///< Defines the revision of the EFI_LOADED_IMAGE_PROTOCOL structure.
                                    ///< All future revisions will be backward compatible to the current revision.
    EFI_HANDLE        ParentHandle;   ///< Parent image's image handle. NULL if the image is loaded directly from
                                    ///< the firmware's boot manager.
    EFI_SYSTEM_TABLE  *SystemTable;   ///< the image's EFI system table pointer.

    //
    // Source location of image
    //
    EFI_HANDLE        DeviceHandle;   ///< The device handle that the EFI Image was loaded from.
    EFI_DEVICE_PATH_PROTOCOL  *FilePath;  ///< A pointer to the file path portion specific to DeviceHandle
                                        ///< that the EFI Image was loaded from.
    VOID              *Reserved;      ///< Reserved. DO NOT USE.

    //
    // Images load options
    //
    UINT32            LoadOptionsSize;///< The size in bytes of LoadOptions.
    VOID              *LoadOptions;   ///< A pointer to the image's binary load options.

    //
    // Location of where image was loaded
    //
    VOID              *ImageBase;     ///< The base address at which the image was loaded.
    UINT64            ImageSize;      ///< The size in bytes of the loaded image.
    EFI_MEMORY_TYPE   ImageCodeType;  ///< The memory type that the code sections were loaded as.
    EFI_MEMORY_TYPE   ImageDataType;  ///< The memory type that the data sections were loaded as.
    EFI_IMAGE_UNLOAD  Unload;
};

//
//

/**
  Reset the Block Device.
  @param  This                 Indicates a pointer to the calling context.
  @param  ExtendedVerification Driver may perform diagnostics on reset.
  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
                               not be reset.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_RESET)(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN BOOLEAN                        ExtendedVerification
    );

/**
  Read BufferSize bytes from Lba into Buffer.
  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read from
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the destination buffer for the data. The caller is
                     responsible for either having implicit or explicit ownership of the buffer.
  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_READ)(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                         MediaId,
    IN EFI_LBA                        Lba,
    IN UINTN                          BufferSize,
    OUT VOID                          *Buffer
    );

/**
  Write BufferSize bytes from Lba into Buffer.
  @param  This       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. The caller is
                     responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  Buffer     A pointer to the source buffer for the data.
  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHNAGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not valid,
                                or the buffer is not on proper alignment.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_WRITE)(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                         MediaId,
    IN EFI_LBA                        Lba,
    IN UINTN                          BufferSize,
    IN VOID                           *Buffer
    );

/**
  Flush the Block Device.
  @param  This              Indicates a pointer to the calling context.
  @retval EFI_SUCCESS       All outstanding data was written to the device
  @retval EFI_DEVICE_ERROR  The device reported an error while writting back the data
  @retval EFI_NO_MEDIA      There is no media in the device.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_FLUSH)(
    IN EFI_BLOCK_IO_PROTOCOL  *This
    );

/**
  Block IO read only mode data and updated only via members of BlockIO
**/
typedef struct {
    ///
    /// The curent media Id. If the media changes, this value is changed.
    ///
    UINT32  MediaId;

    ///
    /// TRUE if the media is removable; otherwise, FALSE.
    ///
    BOOLEAN RemovableMedia;

    ///
    /// TRUE if there is a media currently present in the device;
    /// othersise, FALSE. THis field shows the media present status
    /// as of the most recent ReadBlocks() or WriteBlocks() call.
    ///
    BOOLEAN MediaPresent;

    ///
    /// TRUE if LBA 0 is the first block of a partition; otherwise
    /// FALSE. For media with only one partition this would be TRUE.
    ///
    BOOLEAN LogicalPartition;

    ///
    /// TRUE if the media is marked read-only otherwise, FALSE.
    /// This field shows the read-only status as of the most recent WriteBlocks () call.
    ///
    BOOLEAN ReadOnly;

    ///
    /// TRUE if the WriteBlock () function caches write data.
    ///
    BOOLEAN WriteCaching;

    ///
    /// The intrinsic block size of the device. If the media changes, then
    /// this field is updated.
    ///
    UINT32  BlockSize;

    ///
    /// Supplies the alignment requirement for any buffer to read or write block(s).
    ///
    UINT32  IoAlign;

    ///
    /// The last logical block address on the device.
    /// If the media changes, then this field is updated.
    ///
    EFI_LBA LastBlock;

    ///
    /// Only present if EFI_BLOCK_IO_PROTOCOL.Revision is greater than or equal to
    /// EFI_BLOCK_IO_PROTOCOL_REVISION2. Returns the first LBA is aligned to
    /// a physical block boundary.
    ///
    EFI_LBA LowestAlignedLba;

    ///
    /// Only present if EFI_BLOCK_IO_PROTOCOL.Revision is greater than or equal to
    /// EFI_BLOCK_IO_PROTOCOL_REVISION2. Returns the number of logical blocks
    /// per physical block.
    ///
    UINT32 LogicalBlocksPerPhysicalBlock;

    ///
    /// Only present if EFI_BLOCK_IO_PROTOCOL.Revision is greater than or equal to
    /// EFI_BLOCK_IO_PROTOCOL_REVISION3. Returns the optimal transfer length
    /// granularity as a number of logical blocks.
    ///
    UINT32 OptimalTransferLengthGranularity;
} EFI_BLOCK_IO_MEDIA;

#define EFI_BLOCK_IO_PROTOCOL_REVISION  0x00010000
#define EFI_BLOCK_IO_PROTOCOL_REVISION2 0x00020001
#define EFI_BLOCK_IO_PROTOCOL_REVISION3 0x00020031

///
/// Revision defined in EFI1.1.
///
#define EFI_BLOCK_IO_INTERFACE_REVISION   EFI_BLOCK_IO_PROTOCOL_REVISION

///
///  This protocol provides control over block devices.
///
struct EFI_BLOCK_IO_PROTOCOL {
    ///
    /// The revision to which the block IO interface adheres. All future
    /// revisions must be backwards compatible. If a future version is not
    /// back wards compatible, it is not the same GUID.
    ///
    UINT64              Revision;
    ///
    /// Pointer to the EFI_BLOCK_IO_MEDIA data for this device.
    ///
    EFI_BLOCK_IO_MEDIA  *Media;

    EFI_BLOCK_RESET     Reset;
    EFI_BLOCK_READ      ReadBlocks;
    EFI_BLOCK_WRITE     WriteBlocks;
    EFI_BLOCK_FLUSH     FlushBlocks;
};

//
//

/**
  Open the root directory on a volume.
  @param  This A pointer to the volume to open the root directory.
  @param  Root A pointer to the location to return the opened file handle for the
               root directory.
  @retval EFI_SUCCESS          The device was opened.
  @retval EFI_UNSUPPORTED      This volume does not support the requested file system type.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES The volume was not opened due to lack of resources.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported. Any existing file handles for this volume are
                               no longer valid. To access the files on the new medium, the
                               volume must be reopened with OpenVolume().
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
    OUT EFI_FILE_PROTOCOL                 **Root
    );

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION  0x00010000

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    ///
    /// The version of the EFI_SIMPLE_FILE_SYSTEM_PROTOCOL. The version
    /// specified by this specification is 0x00010000. All future revisions
    /// must be backwards compatible.
    ///
    UINT64                                      Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
};

/**
  Opens a new file relative to the source file's location.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to the source location. This would typically be an open
                     handle to a directory.
  @param  NewHandle  A pointer to the location to return the opened handle for the new
                     file.
  @param  FileName   The Null-terminated string of the name of the file to be opened.
                     The file name may contain the following path modifiers: "\", ".",
                     and "..".
  @param  OpenMode   The mode to open the file. The only valid combinations that the
                     file may be opened with are: Read, Read/Write, or Create/Read/Write.
  @param  Attributes Only valid for EFI_FILE_MODE_CREATE, in which case these are the
                     attribute bits for the newly created file.
  @retval EFI_SUCCESS          The file was opened.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN)(
    IN EFI_FILE_PROTOCOL        *This,
    OUT EFI_FILE_PROTOCOL       **NewHandle,
    IN CHAR16          const    *FileName,    // <-- DGOS: hacked in const
    IN UINT64                   OpenMode,
    IN UINT64                   Attributes
    );

//
// Open modes
//
#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

//
// File attributes
//
#define EFI_FILE_READ_ONLY  0x0000000000000001ULL
#define EFI_FILE_HIDDEN     0x0000000000000002ULL
#define EFI_FILE_SYSTEM     0x0000000000000004ULL
#define EFI_FILE_RESERVED   0x0000000000000008ULL
#define EFI_FILE_DIRECTORY  0x0000000000000010ULL
#define EFI_FILE_ARCHIVE    0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR 0x0000000000000037ULL

/**
  Closes a specified file handle.
  @param  This          A pointer to the EFI_FILE_PROTOCOL instance that is the file
                        handle to close.
  @retval EFI_SUCCESS   The file was closed.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_CLOSE)(
    IN EFI_FILE_PROTOCOL  *This
    );

/**
  Close and delete the file handle.
  @param  This                     A pointer to the EFI_FILE_PROTOCOL instance that is the
                                   handle to the file to delete.
  @retval EFI_SUCCESS              The file was closed and deleted, and the handle was closed.
  @retval EFI_WARN_DELETE_FAILURE  The handle was closed, but the file was not deleted.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_DELETE)(
    IN EFI_FILE_PROTOCOL  *This
    );

/**
  Reads data from a file.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to read data from.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer into which the data is read.
  @retval EFI_SUCCESS          Data was read.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR     On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL The BufferSize is too small to read the current directory
                               entry. BufferSize has been updated with the size
                               needed to complete the request.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ)(
    IN EFI_FILE_PROTOCOL        *This,
    IN OUT UINTN                *BufferSize,
    OUT VOID                    *Buffer
    );

/**
  Writes data to a file.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to write data to.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     actually written. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer of data to write.
  @retval EFI_SUCCESS          Data was written.
  @retval EFI_UNSUPPORTED      Writes to open directory files are not supported.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to write to a deleted file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read only.
  @retval EFI_VOLUME_FULL      The volume is full.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE)(
    IN EFI_FILE_PROTOCOL        *This,
    IN OUT UINTN                *BufferSize,
    IN VOID                     *Buffer
    );

/**
  Sets a file's current position.
  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the
                          file handle to set the requested position on.
  @param  Position        The byte position from the start of the file to set.
  @retval EFI_SUCCESS      The position was set.
  @retval EFI_UNSUPPORTED  The seek request for nonzero is not valid on open
                           directories.
  @retval EFI_DEVICE_ERROR An attempt was made to set the position of a deleted file.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_POSITION)(
    IN EFI_FILE_PROTOCOL        *This,
    IN UINT64                   Position
    );

/**
  Returns a file's current position.
  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle to get the current position on.
  @param  Position        The address to return the file's current position value.
  @retval EFI_SUCCESS      The position was returned.
  @retval EFI_UNSUPPORTED  The request is not valid on open directories.
  @retval EFI_DEVICE_ERROR An attempt was made to get the position from a deleted file.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_POSITION)(
    IN EFI_FILE_PROTOCOL        *This,
    OUT UINT64                  *Position
    );

/**
  Returns information about a file.
  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the requested information is for.
  @param  InformationType The type identifier for the information being requested.
  @param  BufferSize      On input, the size of Buffer. On output, the amount of data
                          returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer          A pointer to the data buffer to return. The buffer's type is
                          indicated by InformationType.
  @retval EFI_SUCCESS          The information was returned.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL The BufferSize is too small to read the current directory entry.
                               BufferSize has been updated with the size needed to complete
                               the request.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_INFO)(
    IN EFI_FILE_PROTOCOL        *This,
    IN EFI_GUID          const  *InformationType,
    IN OUT UINTN                *BufferSize,
    OUT VOID                    *Buffer
    );

/**
  Sets information about a file.
  @param  File            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the information is for.
  @param  InformationType The type identifier for the information being set.
  @param  BufferSize      The size, in bytes, of Buffer.
  @param  Buffer          A pointer to the data buffer to write. The buffer's type is
                          indicated by InformationType.
  @retval EFI_SUCCESS          The information was set.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_INFO_ID and the media is
                               read-only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_PROTOCOL_SYSTEM_INFO_ID
                               and the media is read only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_SYSTEM_VOLUME_LABEL_ID
                               and the media is read-only.
  @retval EFI_ACCESS_DENIED    An attempt is made to change the name of a file to a
                               file that is already present.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the EFI_FILE_DIRECTORY
                               Attribute.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the size of a directory.
  @retval EFI_ACCESS_DENIED    InformationType is EFI_FILE_INFO_ID and the file was opened
                               read-only and an attempt is being made to modify a field
                               other than Attribute.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_BAD_BUFFER_SIZE  BufferSize is smaller than the size of the type indicated
                               by InformationType.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_INFO)(
    IN EFI_FILE_PROTOCOL        *This,
    IN EFI_GUID          const  *InformationType,
    IN UINTN                    BufferSize,
    IN VOID                     *Buffer
    );

/**
  Flushes all modified data associated with a file to a device.
  @param  This A pointer to the EFI_FILE_PROTOCOL instance that is the file
               handle to flush.
  @retval EFI_SUCCESS          The data was flushed.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read-only.
  @retval EFI_VOLUME_FULL      The volume is full.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH)(
    IN EFI_FILE_PROTOCOL  *This
    );

struct EFI_FILE_IO_TOKEN {
    //
    // If Event is NULL, then blocking I/O is performed.
    // If Event is not NULL and non-blocking I/O is supported, then non-blocking I/O is performed,
    // and Event will be signaled when the read request is completed.
    // The caller must be prepared to handle the case where the callback associated with Event
    // occurs before the original asynchronous I/O request call returns.
    //
    EFI_EVENT                   Event;

    //
    // Defines whether or not the signaled event encountered an error.
    //
    EFI_STATUS                  Status;

    //
    // For OpenEx():  Not Used, ignored.
    // For ReadEx():  On input, the size of the Buffer. On output, the amount of data returned in Buffer.
    //                In both cases, the size is measured in bytes.
    // For WriteEx(): On input, the size of the Buffer. On output, the amount of data actually written.
    //                In both cases, the size is measured in bytes.
    // For FlushEx(): Not used, ignored.
    //
    UINTN                       BufferSize;

    //
    // For OpenEx():  Not Used, ignored.
    // For ReadEx():  The buffer into which the data is read.
    // For WriteEx(): The buffer of data to write.
    // For FlushEx(): Not Used, ignored.
    //
    VOID                        *Buffer;
};

/**
  Opens a new file relative to the source directory's location.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to the source location.
  @param  NewHandle  A pointer to the location to return the opened handle for the new
                     file.
  @param  FileName   The Null-terminated string of the name of the file to be opened.
                     The file name may contain the following path modifiers: "\", ".",
                     and "..".
  @param  OpenMode   The mode to open the file. The only valid combinations that the
                     file may be opened with are: Read, Read/Write, or Create/Read/Write.
  @param  Attributes Only valid for EFI_FILE_MODE_CREATE, in which case these are the
                     attribute bits for the newly created file.
  @param  Token      A pointer to the token associated with the transaction.
  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN_EX)(
    IN EFI_FILE_PROTOCOL        *This,
    OUT EFI_FILE_PROTOCOL       **NewHandle,
    IN CHAR16                   *FileName,
    IN UINT64                   OpenMode,
    IN UINT64                   Attributes,
    IN OUT EFI_FILE_IO_TOKEN    *Token
    );


/**
  Reads data from a file.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to read data from.
  @param  Token      A pointer to the token associated with the transaction.
  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR     On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ_EX) (
    IN EFI_FILE_PROTOCOL        *This,
    IN OUT EFI_FILE_IO_TOKEN    *Token
    );


/**
  Writes data to a file.
  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to write data to.
  @param  Token      A pointer to the token associated with the transaction.
  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_UNSUPPORTED      Writes to open directory files are not supported.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to write to a deleted file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read only.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE_EX) (
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT EFI_FILE_IO_TOKEN    *Token
);

/**
  Flushes all modified data associated with a file to a device.
  @param  This  A pointer to the EFI_FILE_PROTOCOL instance that is the file
                handle to flush.
  @param  Token A pointer to the token associated with the transaction.
  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read-only.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH_EX) (
    IN EFI_FILE_PROTOCOL        *This,
    IN OUT EFI_FILE_IO_TOKEN    *Token
    );

#define EFI_FILE_PROTOCOL_REVISION        0x00010000
#define EFI_FILE_PROTOCOL_REVISION2       0x00020000
#define EFI_FILE_PROTOCOL_LATEST_REVISION EFI_FILE_PROTOCOL_REVISION2

//
// Revision defined in EFI1.1.
//
#define EFI_FILE_REVISION   EFI_FILE_PROTOCOL_REVISION

///
/// The EFI_FILE_PROTOCOL provides file IO access to supported file systems.
/// An EFI_FILE_PROTOCOL provides access to a file's or directory's contents,
/// and is also a reference to a location in the directory tree of the file system
/// in which the file resides. With any given file handle, other files may be opened
/// relative to this file's location, yielding new file handles.
///
struct EFI_FILE_PROTOCOL {
    ///
    /// The version of the EFI_FILE_PROTOCOL interface. The version specified
    /// by this specification is EFI_FILE_PROTOCOL_LATEST_REVISION.
    /// Future versions are required to be backward compatible to version 1.0.
    ///
    UINT64                Revision;
    EFI_FILE_OPEN         Open;
    EFI_FILE_CLOSE        Close;
    EFI_FILE_DELETE       Delete;
    EFI_FILE_READ         Read;
    EFI_FILE_WRITE        Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO     GetInfo;
    EFI_FILE_SET_INFO     SetInfo;
    EFI_FILE_FLUSH        Flush;
    EFI_FILE_OPEN_EX      OpenEx;
    EFI_FILE_READ_EX      ReadEx;
    EFI_FILE_WRITE_EX     WriteEx;
    EFI_FILE_FLUSH_EX     FlushEx;
};

//
//

typedef struct _EFI_FILE_PROTOCOL         *EFI_FILE_HANDLE;

///
/// Protocol GUID name defined in EFI1.1.
///
#define SIMPLE_FILE_SYSTEM_PROTOCOL       EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID

///
/// Protocol name defined in EFI1.1.
///
typedef EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   EFI_FILE_IO_INTERFACE;
typedef EFI_FILE_PROTOCOL                 EFI_FILE;

///
/// Revision defined in EFI1.1
///
#define EFI_FILE_IO_INTERFACE_REVISION  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION

#define EFI_FILE_PROTOCOL_REVISION        0x00010000
#define EFI_FILE_PROTOCOL_REVISION2       0x00020000
#define EFI_FILE_PROTOCOL_LATEST_REVISION EFI_FILE_PROTOCOL_REVISION2

//
// Revision defined in EFI1.1.
//
#define EFI_FILE_REVISION   EFI_FILE_PROTOCOL_REVISION

///
/// The EFI_FILE_PROTOCOL provides file IO access to supported file systems.
/// An EFI_FILE_PROTOCOL provides access to a file's or directory's contents,
/// and is also a reference to a location in the directory tree of the file system
/// in which the file resides. With any given file handle, other files may be opened
/// relative to this file's location, yielding new file handles.
///
struct _EFI_FILE_PROTOCOL {
  ///
  /// The version of the EFI_FILE_PROTOCOL interface. The version specified
  /// by this specification is EFI_FILE_PROTOCOL_LATEST_REVISION.
  /// Future versions are required to be backward compatible to version 1.0.
  ///
  UINT64                Revision;
  EFI_FILE_OPEN         Open;
  EFI_FILE_CLOSE        Close;
  EFI_FILE_DELETE       Delete;
  EFI_FILE_READ         Read;
  EFI_FILE_WRITE        Write;
  EFI_FILE_GET_POSITION GetPosition;
  EFI_FILE_SET_POSITION SetPosition;
  EFI_FILE_GET_INFO     GetInfo;
  EFI_FILE_SET_INFO     SetInfo;
  EFI_FILE_FLUSH        Flush;
  EFI_FILE_OPEN_EX      OpenEx;
  EFI_FILE_READ_EX      ReadEx;
  EFI_FILE_WRITE_EX     WriteEx;
  EFI_FILE_FLUSH_EX     FlushEx;
};

typedef struct {
    UINT64 Size;
    BOOLEAN ReadOnly;
    UINT64 VolumeSize;
    UINT64 FreeSpace;
    UINT32 BlockSize;
    CHAR16 VolumeLabel[1];
} EFI_FILE_SYSTEM_INFO;

typedef struct {
    CHAR16 VolumeLabel[1];
} EFI_FILE_SYSTEM_VOLUME_LABEL;


#define EFI_FILE_SYSTEM_INFO_GUID { \
    0x09576e93, 0x6d3f, 0x11d2, { \
        0x8e, 0x39,0x00,0xa0,0xc9,0x69,0x72, 0x3b \
    } \
}

//
// EFI PXE Base Code

///
/// PXE Base Code protocol.
///
#define EFI_PXE_BASE_CODE_PROTOCOL_GUID \
{ \
    0x03c4e603, 0xac28, 0x11d3, { \
        0x9a, 0x2d, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d \
    } \
}

typedef struct _EFI_PXE_BASE_CODE_PROTOCOL  EFI_PXE_BASE_CODE_PROTOCOL;

///
/// Protocol defined in EFI1.1.
///
typedef EFI_PXE_BASE_CODE_PROTOCOL  EFI_PXE_BASE_CODE;

///
/// Default IP TTL and ToS.
///
#define DEFAULT_TTL 16
#define DEFAULT_ToS 0

///
/// ICMP error format.
///
typedef struct {
    UINT8   Type;
    UINT8   Code;
    UINT16  Checksum;
    union {
        UINT32  reserved;
        UINT32  Mtu;
        UINT32  Pointer;
        struct {
            UINT16  Identifier;
            UINT16  Sequence;
        } Echo;
    } u;
    UINT8 Data[494];
} EFI_PXE_BASE_CODE_ICMP_ERROR;

///
/// TFTP error format.
///
typedef struct {
    UINT8 ErrorCode;
    CHAR8 ErrorString[127];
} EFI_PXE_BASE_CODE_TFTP_ERROR;

///
/// IP Receive Filter definitions.
///
#define EFI_PXE_BASE_CODE_MAX_IPCNT 8

///
/// IP Receive Filter structure.
///
typedef struct {
    UINT8           Filters;
    UINT8           IpCnt;
    UINT16          reserved;
    EFI_IP_ADDRESS  IpList[EFI_PXE_BASE_CODE_MAX_IPCNT];
} EFI_PXE_BASE_CODE_IP_FILTER;

#define EFI_PXE_BASE_CODE_IP_FILTER_STATION_IP            0x0001
#define EFI_PXE_BASE_CODE_IP_FILTER_BROADCAST             0x0002
#define EFI_PXE_BASE_CODE_IP_FILTER_PROMISCUOUS           0x0004
#define EFI_PXE_BASE_CODE_IP_FILTER_PROMISCUOUS_MULTICAST 0x0008

///
/// ARP cache entries.
///
typedef struct {
    EFI_IP_ADDRESS  IpAddr;
    EFI_MAC_ADDRESS MacAddr;
} EFI_PXE_BASE_CODE_ARP_ENTRY;

///
/// ARP route table entries.
///
typedef struct {
    EFI_IP_ADDRESS  IpAddr;
    EFI_IP_ADDRESS  SubnetMask;
    EFI_IP_ADDRESS  GwAddr;
} EFI_PXE_BASE_CODE_ROUTE_ENTRY;

//
// UDP definitions
//
typedef UINT16  EFI_PXE_BASE_CODE_UDP_PORT;

#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_SRC_IP    0x0001
#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_SRC_PORT  0x0002
#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_DEST_IP   0x0004
#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_DEST_PORT 0x0008
#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_USE_FILTER    0x0010
#define EFI_PXE_BASE_CODE_UDP_OPFLAGS_MAY_FRAGMENT  0x0020

//
// Discover() definitions
//
#define EFI_PXE_BASE_CODE_BOOT_TYPE_BOOTSTRAP         0
#define EFI_PXE_BASE_CODE_BOOT_TYPE_MS_WINNT_RIS      1
#define EFI_PXE_BASE_CODE_BOOT_TYPE_INTEL_LCM         2
#define EFI_PXE_BASE_CODE_BOOT_TYPE_DOSUNDI           3
#define EFI_PXE_BASE_CODE_BOOT_TYPE_NEC_ESMPRO        4
#define EFI_PXE_BASE_CODE_BOOT_TYPE_IBM_WSoD          5
#define EFI_PXE_BASE_CODE_BOOT_TYPE_IBM_LCCM          6
#define EFI_PXE_BASE_CODE_BOOT_TYPE_CA_UNICENTER_TNG  7
#define EFI_PXE_BASE_CODE_BOOT_TYPE_HP_OPENVIEW       8
#define EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_9         9
#define EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_10        10
#define EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_11        11
#define EFI_PXE_BASE_CODE_BOOT_TYPE_NOT_USED_12       12
#define EFI_PXE_BASE_CODE_BOOT_TYPE_REDHAT_INSTALL    13
#define EFI_PXE_BASE_CODE_BOOT_TYPE_REDHAT_BOOT       14
#define EFI_PXE_BASE_CODE_BOOT_TYPE_REMBO             15
#define EFI_PXE_BASE_CODE_BOOT_TYPE_BEOBOOT           16
//
// 17 through 32767 are reserved
// 32768 through 65279 are for vendor use
// 65280 through 65534 are reserved
//
#define EFI_PXE_BASE_CODE_BOOT_TYPE_PXETEST   65535

#define EFI_PXE_BASE_CODE_BOOT_LAYER_MASK     0x7FFF
#define EFI_PXE_BASE_CODE_BOOT_LAYER_INITIAL  0x0000

//
// PXE Tag definition that identifies the processor
// and programming environment of the client system.
// These identifiers are defined by IETF:
// http://www.ietf.org/assignments/dhcpv6-parameters/dhcpv6-parameters.xml
//
#if defined (MDE_CPU_IA32)
#define EFI_PXE_CLIENT_SYSTEM_ARCHITECTURE    0x0006
#elif defined (MDE_CPU_IPF)
#define EFI_PXE_CLIENT_SYSTEM_ARCHITECTURE    0x0002
#elif defined (MDE_CPU_X64)
#define EFI_PXE_CLIENT_SYSTEM_ARCHITECTURE    0x0007
#elif defined (MDE_CPU_ARM)
#define EFI_PXE_CLIENT_SYSTEM_ARCHITECTURE    0x000A
#elif defined (MDE_CPU_AARCH64)
#define EFI_PXE_CLIENT_SYSTEM_ARCHITECTURE    0x000B
#endif


///
/// Discover() server list structure.
///
typedef struct {
    UINT16          Type;
    BOOLEAN         AcceptAnyResponse;
    UINT8           Reserved;
    EFI_IP_ADDRESS  IpAddr;
} EFI_PXE_BASE_CODE_SRVLIST;

///
/// Discover() information override structure.
///
typedef struct {
    BOOLEAN                   UseMCast;
    BOOLEAN                   UseBCast;
    BOOLEAN                   UseUCast;
    BOOLEAN                   MustUseList;
    EFI_IP_ADDRESS            ServerMCastIp;
    UINT16                    IpCnt;
    EFI_PXE_BASE_CODE_SRVLIST SrvList[1];
} EFI_PXE_BASE_CODE_DISCOVER_INFO;

///
/// TFTP opcode definitions.
///
typedef enum {
    EFI_PXE_BASE_CODE_TFTP_FIRST,
    EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
    EFI_PXE_BASE_CODE_TFTP_READ_FILE,
    EFI_PXE_BASE_CODE_TFTP_WRITE_FILE,
    EFI_PXE_BASE_CODE_TFTP_READ_DIRECTORY,
    EFI_PXE_BASE_CODE_MTFTP_GET_FILE_SIZE,
    EFI_PXE_BASE_CODE_MTFTP_READ_FILE,
    EFI_PXE_BASE_CODE_MTFTP_READ_DIRECTORY,
    EFI_PXE_BASE_CODE_MTFTP_LAST
} EFI_PXE_BASE_CODE_TFTP_OPCODE;

///
/// MTFTP information. This information is required
/// to start or join a multicast TFTP session. It is also required to
/// perform the "get file size" and "read directory" operations of MTFTP.
///
typedef struct {
    EFI_IP_ADDRESS              MCastIp;
    EFI_PXE_BASE_CODE_UDP_PORT  CPort;
    EFI_PXE_BASE_CODE_UDP_PORT  SPort;
    UINT16                      ListenTimeout;
    UINT16                      TransmitTimeout;
} EFI_PXE_BASE_CODE_MTFTP_INFO;

///
/// DHCPV4 Packet structure.
///
typedef struct {
    UINT8   BootpOpcode;
    UINT8   BootpHwType;
    UINT8   BootpHwAddrLen;
    UINT8   BootpGateHops;
    UINT32  BootpIdent;
    UINT16  BootpSeconds;
    UINT16  BootpFlags;
    UINT8   BootpCiAddr[4];
    UINT8   BootpYiAddr[4];
    UINT8   BootpSiAddr[4];
    UINT8   BootpGiAddr[4];
    UINT8   BootpHwAddr[16];
    UINT8   BootpSrvName[64];
    UINT8   BootpBootFile[128];
    UINT32  DhcpMagik;
    UINT8   DhcpOptions[56];
} EFI_PXE_BASE_CODE_DHCPV4_PACKET;

///
/// DHCPV6 Packet structure.
///
typedef struct {
    UINT32  MessageType:8;
    UINT32  TransactionId:24;
    UINT8   DhcpOptions[1024];
} _ms_struct EFI_PXE_BASE_CODE_DHCPV6_PACKET;

///
/// Packet structure.
///
typedef union {
    UINT8                           Raw[1472];
    EFI_PXE_BASE_CODE_DHCPV4_PACKET Dhcpv4;
    EFI_PXE_BASE_CODE_DHCPV6_PACKET Dhcpv6;
} EFI_PXE_BASE_CODE_PACKET;

//
// PXE Base Code Mode structure
//
#define EFI_PXE_BASE_CODE_MAX_ARP_ENTRIES   8
#define EFI_PXE_BASE_CODE_MAX_ROUTE_ENTRIES 8

///
/// EFI_PXE_BASE_CODE_MODE.
/// The data values in this structure are read-only and
/// are updated by the code that produces the
/// EFI_PXE_BASE_CODE_PROTOCOL functions.
///
typedef struct {
    BOOLEAN                       Started;
    BOOLEAN                       Ipv6Available;
    BOOLEAN                       Ipv6Supported;
    BOOLEAN                       UsingIpv6;
    BOOLEAN                       BisSupported;
    BOOLEAN                       BisDetected;
    BOOLEAN                       AutoArp;
    BOOLEAN                       SendGUID;
    BOOLEAN                       DhcpDiscoverValid;
    BOOLEAN                       DhcpAckReceived;
    BOOLEAN                       ProxyOfferReceived;
    BOOLEAN                       PxeDiscoverValid;
    BOOLEAN                       PxeReplyReceived;
    BOOLEAN                       PxeBisReplyReceived;
    BOOLEAN                       IcmpErrorReceived;
    BOOLEAN                       TftpErrorReceived;
    BOOLEAN                       MakeCallbacks;
    UINT8                         TTL;
    UINT8                         ToS;
    UINT8  hack__;    // DGOS: added this field to make this structure work
    EFI_IP_ADDRESS                StationIp;
    EFI_IP_ADDRESS                SubnetMask;
    EFI_PXE_BASE_CODE_PACKET      DhcpDiscover;
    EFI_PXE_BASE_CODE_PACKET      DhcpAck;
    EFI_PXE_BASE_CODE_PACKET      ProxyOffer;
    EFI_PXE_BASE_CODE_PACKET      PxeDiscover;
    EFI_PXE_BASE_CODE_PACKET      PxeReply;
    EFI_PXE_BASE_CODE_PACKET      PxeBisReply;
    EFI_PXE_BASE_CODE_IP_FILTER   IpFilter;
    UINT32                        ArpCacheEntries;
    EFI_PXE_BASE_CODE_ARP_ENTRY   ArpCache[EFI_PXE_BASE_CODE_MAX_ARP_ENTRIES];
    UINT32                        RouteTableEntries;
    EFI_PXE_BASE_CODE_ROUTE_ENTRY RouteTable[EFI_PXE_BASE_CODE_MAX_ROUTE_ENTRIES];
    EFI_PXE_BASE_CODE_ICMP_ERROR  IcmpError;
    EFI_PXE_BASE_CODE_TFTP_ERROR  TftpError;
} _ms_struct _packed EFI_PXE_BASE_CODE_MODE;

//
// PXE Base Code Interface Function definitions
//

/**
  Enables the use of the PXE Base Code Protocol functions.
  This function enables the use of the PXE Base Code Protocol functions. If the
  Started field of the EFI_PXE_BASE_CODE_MODE structure is already TRUE, then
  EFI_ALREADY_STARTED will be returned. If UseIpv6 is TRUE, then IPv6 formatted
  addresses will be used in this session. If UseIpv6 is FALSE, then IPv4 formatted
  addresses will be used in this session. If UseIpv6 is TRUE, and the Ipv6Supported
  field of the EFI_PXE_BASE_CODE_MODE structure is FALSE, then EFI_UNSUPPORTED will
  be returned. If there is not enough memory or other resources to start the PXE
  Base Code Protocol, then EFI_OUT_OF_RESOURCES will be returned. Otherwise, the
  PXE Base Code Protocol will be started, and all of the fields of the EFI_PXE_BASE_CODE_MODE
  structure will be initialized as follows:
    StartedSet to TRUE.
    Ipv6SupportedUnchanged.
    Ipv6AvailableUnchanged.
    UsingIpv6Set to UseIpv6.
    BisSupportedUnchanged.
    BisDetectedUnchanged.
    AutoArpSet to TRUE.
    SendGUIDSet to FALSE.
    TTLSet to DEFAULT_TTL.
    ToSSet to DEFAULT_ToS.
    DhcpCompletedSet to FALSE.
    ProxyOfferReceivedSet to FALSE.
    StationIpSet to an address of all zeros.
    SubnetMaskSet to a subnet mask of all zeros.
    DhcpDiscoverZero-filled.
    DhcpAckZero-filled.
    ProxyOfferZero-filled.
    PxeDiscoverValidSet to FALSE.
    PxeDiscoverZero-filled.
    PxeReplyValidSet to FALSE.
    PxeReplyZero-filled.
    PxeBisReplyValidSet to FALSE.
    PxeBisReplyZero-filled.
    IpFilterSet the Filters field to 0 and the IpCnt field to 0.
    ArpCacheEntriesSet to 0.
    ArpCacheZero-filled.
    RouteTableEntriesSet to 0.
    RouteTableZero-filled.
    IcmpErrorReceivedSet to FALSE.
    IcmpErrorZero-filled.
    TftpErroReceivedSet to FALSE.
    TftpErrorZero-filled.
    MakeCallbacksSet to TRUE if the PXE Base Code Callback Protocol is available.
    Set to FALSE if the PXE Base Code Callback Protocol is not available.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  UseIpv6               Specifies the type of IP addresses that are to be used during the session
                                that is being started. Set to TRUE for IPv6 addresses, and FALSE for
                                IPv4 addresses.

  @retval EFI_SUCCESS           The PXE Base Code Protocol was started.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this oper
  @retval EFI_UNSUPPORTED       UseIpv6 is TRUE, but the Ipv6Supported field of the
                                EFI_PXE_BASE_CODE_MODE structure is FALSE.
  @retval EFI_ALREADY_STARTED   The PXE Base Code Protocol is already in the started state.
  @retval EFI_INVALID_PARAMETER The This parameter is NULL or does not point to a valid
                                EFI_PXE_BASE_CODE_PROTOCOL structure.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough memory or other resources to start the
                                PXE Base Code Protocol.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_START)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN BOOLEAN                               UseIpv6
        );

/**
  Disables the use of the PXE Base Code Protocol functions.
  This function stops all activity on the network device. All the resources allocated
  in Start() are released, the Started field of the EFI_PXE_BASE_CODE_MODE structure is
  set to FALSE and EFI_SUCCESS is returned. If the Started field of the EFI_PXE_BASE_CODE_MODE
  structure is already FALSE, then EFI_NOT_STARTED will be returned.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.

  @retval EFI_SUCCESS           The PXE Base Code Protocol was stopped.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is already in the stopped state.
  @retval EFI_INVALID_PARAMETER The This parameter is NULL or does not point to a valid
                                EFI_PXE_BASE_CODE_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_STOP)(
        IN EFI_PXE_BASE_CODE_PROTOCOL    *This
        );

/**
  Attempts to complete a DHCPv4 D.O.R.A. (discover / offer / request / acknowledge) or DHCPv6
  S.A.R.R (solicit / advertise / request / reply) sequence.
  This function attempts to complete the DHCP sequence. If this sequence is completed,
  then EFI_SUCCESS is returned, and the DhcpCompleted, ProxyOfferReceived, StationIp,
  SubnetMask, DhcpDiscover, DhcpAck, and ProxyOffer fields of the EFI_PXE_BASE_CODE_MODE
  structure are filled in.
  If SortOffers is TRUE, then the cached DHCP offer packets will be sorted before
  they are tried. If SortOffers is FALSE, then the cached DHCP offer packets will
  be tried in the order in which they are received. Please see the Preboot Execution
  Environment (PXE) Specification for additional details on the implementation of DHCP.
  This function can take at least 31 seconds to timeout and return control to the
  caller. If the DHCP sequence does not complete, then EFI_TIMEOUT will be returned.
  If the Callback Protocol does not return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE,
  then the DHCP sequence will be stopped and EFI_ABORTED will be returned.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  SortOffers            TRUE if the offers received should be sorted. Set to FALSE to try the
                                offers in the order that they are received.

  @retval EFI_SUCCESS           Valid DHCP has completed.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER The This parameter is NULL or does not point to a valid
                                EFI_PXE_BASE_CODE_PROTOCOL structure.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough memory to complete the DHCP Protocol.
  @retval EFI_ABORTED           The callback function aborted the DHCP Protocol.
  @retval EFI_TIMEOUT           The DHCP Protocol timed out.
  @retval EFI_ICMP_ERROR        An ICMP error packet was received during the DHCP session.
  @retval EFI_NO_RESPONSE       Valid PXE offer was not received.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_DHCP)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN BOOLEAN                               SortOffers
        );

/**
  Attempts to complete the PXE Boot Server and/or boot image discovery sequence.
  This function attempts to complete the PXE Boot Server and/or boot image discovery
  sequence. If this sequence is completed, then EFI_SUCCESS is returned, and the
  PxeDiscoverValid, PxeDiscover, PxeReplyReceived, and PxeReply fields of the
  EFI_PXE_BASE_CODE_MODE structure are filled in. If UseBis is TRUE, then the
  PxeBisReplyReceived and PxeBisReply fields of the EFI_PXE_BASE_CODE_MODE structure
  will also be filled in. If UseBis is FALSE, then PxeBisReplyValid will be set to FALSE.
  In the structure referenced by parameter Info, the PXE Boot Server list, SrvList[],
  has two uses: It is the Boot Server IP address list used for unicast discovery
  (if the UseUCast field is TRUE), and it is the list used for Boot Server verification
  (if the MustUseList field is TRUE). Also, if the MustUseList field in that structure
  is TRUE and the AcceptAnyResponse field in the SrvList[] array is TRUE, any Boot
  Server reply of that type will be accepted. If the AcceptAnyResponse field is
  FALSE, only responses from Boot Servers with matching IP addresses will be accepted.
  This function can take at least 10 seconds to timeout and return control to the
  caller. If the Discovery sequence does not complete, then EFI_TIMEOUT will be
  returned. Please see the Preboot Execution Environment (PXE) Specification for
  additional details on the implementation of the Discovery sequence.
  If the Callback Protocol does not return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE,
  then the Discovery sequence is stopped and EFI_ABORTED will be returned.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  Type                  The type of bootstrap to perform.
  @param  Layer                 The pointer to the boot server layer number to discover, which must be
                                PXE_BOOT_LAYER_INITIAL when a new server type is being
                                discovered.
  @param  UseBis                TRUE if Boot Integrity Services are to be used. FALSE otherwise.
  @param  Info                  The pointer to a data structure that contains additional information on the
                                type of discovery operation that is to be performed.

  @retval EFI_SUCCESS           The Discovery sequence has been completed.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.
  @retval EFI_OUT_OF_RESOURCES  Could not allocate enough memory to complete Discovery.
  @retval EFI_ABORTED           The callback function aborted the Discovery sequence.
  @retval EFI_TIMEOUT           The Discovery sequence timed out.
  @retval EFI_ICMP_ERROR        An ICMP error packet was received during the PXE discovery
                                session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_DISCOVER)(
        IN EFI_PXE_BASE_CODE_PROTOCOL           *This,
        IN UINT16                               Type,
        IN UINT16                               *Layer,
        IN BOOLEAN                              UseBis,
        IN EFI_PXE_BASE_CODE_DISCOVER_INFO      *Info   OPTIONAL
        );

/**
  Used to perform TFTP and MTFTP services.
  This function is used to perform TFTP and MTFTP services. This includes the
  TFTP operations to get the size of a file, read a directory, read a file, and
  write a file. It also includes the MTFTP operations to get the size of a file,
  read a directory, and read a file. The type of operation is specified by Operation.
  If the callback function that is invoked during the TFTP/MTFTP operation does
  not return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE, then EFI_ABORTED will
  be returned.
  For read operations, the return data will be placed in the buffer specified by
  BufferPtr. If BufferSize is too small to contain the entire downloaded file,
  then EFI_BUFFER_TOO_SMALL will be returned and BufferSize will be set to zero
  or the size of the requested file (the size of the requested file is only returned
  if the TFTP server supports TFTP options). If BufferSize is large enough for the
  read operation, then BufferSize will be set to the size of the downloaded file,
  and EFI_SUCCESS will be returned. Applications using the PxeBc.Mtftp() services
  should use the get-file-size operations to determine the size of the downloaded
  file prior to using the read-file operations--especially when downloading large
  (greater than 64 MB) files--instead of making two calls to the read-file operation.
  Following this recommendation will save time if the file is larger than expected
  and the TFTP server does not support TFTP option extensions. Without TFTP option
  extension support, the client has to download the entire file, counting and discarding
  the received packets, to determine the file size.
  For write operations, the data to be sent is in the buffer specified by BufferPtr.
  BufferSize specifies the number of bytes to send. If the write operation completes
  successfully, then EFI_SUCCESS will be returned.
  For TFTP "get file size" operations, the size of the requested file or directory
  is returned in BufferSize, and EFI_SUCCESS will be returned. If the TFTP server
  does not support options, the file will be downloaded into a bit bucket and the
  length of the downloaded file will be returned. For MTFTP "get file size" operations,
  if the MTFTP server does not support the "get file size" option, EFI_UNSUPPORTED
  will be returned.
  This function can take up to 10 seconds to timeout and return control to the caller.
  If the TFTP sequence does not complete, EFI_TIMEOUT will be returned.
  If the Callback Protocol does not return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE,
  then the TFTP sequence is stopped and EFI_ABORTED will be returned.
  The format of the data returned from a TFTP read directory operation is a null-terminated
  filename followed by a null-terminated information string, of the form
  "size year-month-day hour:minute:second" (i.e. %d %d-%d-%d %d:%d:%f - note that
  the seconds field can be a decimal number), where the date and time are UTC. For
  an MTFTP read directory command, there is additionally a null-terminated multicast
  IP address preceding the filename of the form %d.%d.%d.%d for IP v4. The final
  entry is itself null-terminated, so that the final information string is terminated
  with two null octets.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  Operation             The type of operation to perform.
  @param  BufferPtr             A pointer to the data buffer.
  @param  Overwrite             Only used on write file operations. TRUE if a file on a remote server can
                                be overwritten.
  @param  BufferSize            For get-file-size operations, *BufferSize returns the size of the
                                requested file.
  @param  BlockSize             The requested block size to be used during a TFTP transfer.
  @param  ServerIp              The TFTP / MTFTP server IP address.
  @param  Filename              A Null-terminated ASCII string that specifies a directory name or a file
                                name.
  @param  Info                  The pointer to the MTFTP information.
  @param  DontUseBuffer         Set to FALSE for normal TFTP and MTFTP read file operation.

  @retval EFI_SUCCESS           The TFTP/MTFTP operation was completed.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.
  @retval EFI_BUFFER_TOO_SMALL  The buffer is not large enough to complete the read operation.
  @retval EFI_ABORTED           The callback function aborted the TFTP/MTFTP operation.
  @retval EFI_TIMEOUT           The TFTP/MTFTP operation timed out.
  @retval EFI_ICMP_ERROR        An ICMP error packet was received during the MTFTP session.
  @retval EFI_TFTP_ERROR        A TFTP error packet was received during the MTFTP session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_MTFTP)(
        IN EFI_PXE_BASE_CODE_PROTOCOL                *This,
        IN EFI_PXE_BASE_CODE_TFTP_OPCODE             Operation,
        IN OUT VOID                                  *BufferPtr OPTIONAL,
        IN BOOLEAN                                   Overwrite,
        IN OUT UINT64                                *BufferSize,
        IN UINTN                                     *BlockSize OPTIONAL,
        IN EFI_IP_ADDRESS                            *ServerIp,
        IN UINT8                                     *Filename  OPTIONAL,
        IN EFI_PXE_BASE_CODE_MTFTP_INFO              *Info      OPTIONAL,
        IN BOOLEAN                                   DontUseBuffer
        );

/**
  Writes a UDP packet to the network interface.
  This function writes a UDP packet specified by the (optional HeaderPtr and)
  BufferPtr parameters to the network interface. The UDP header is automatically
  built by this routine. It uses the parameters OpFlags, DestIp, DestPort, GatewayIp,
  SrcIp, and SrcPort to build this header. If the packet is successfully built and
  transmitted through the network interface, then EFI_SUCCESS will be returned.
  If a timeout occurs during the transmission of the packet, then EFI_TIMEOUT will
  be returned. If an ICMP error occurs during the transmission of the packet, then
  the IcmpErrorReceived field is set to TRUE, the IcmpError field is filled in and
  EFI_ICMP_ERROR will be returned. If the Callback Protocol does not return
  EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE, then EFI_ABORTED will be returned.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  OpFlags               The UDP operation flags.
  @param  DestIp                The destination IP address.
  @param  DestPort              The destination UDP port number.
  @param  GatewayIp             The gateway IP address.
  @param  SrcIp                 The source IP address.
  @param  SrcPort               The source UDP port number.
  @param  HeaderSize            An optional field which may be set to the length of a header at
                                HeaderPtr to be prefixed to the data at BufferPtr.
  @param  HeaderPtr             If HeaderSize is not NULL, a pointer to a header to be prefixed to the
                                data at BufferPtr.
  @param  BufferSize            A pointer to the size of the data at BufferPtr.
  @param  BufferPtr             A pointer to the data to be written.

  @retval EFI_SUCCESS           The UDP Write operation was completed.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_BAD_BUFFER_SIZE   The buffer is too long to be transmitted.
  @retval EFI_ABORTED           The callback function aborted the UDP Write operation.
  @retval EFI_TIMEOUT           The UDP Write operation timed out.
  @retval EFI_ICMP_ERROR        An ICMP error packet was received during the UDP write session.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_UDP_WRITE)(
        IN EFI_PXE_BASE_CODE_PROTOCOL                *This,
        IN UINT16                                    OpFlags,
        IN EFI_IP_ADDRESS                            *DestIp,
        IN EFI_PXE_BASE_CODE_UDP_PORT                *DestPort,
        IN EFI_IP_ADDRESS                            *GatewayIp,  OPTIONAL
        IN EFI_IP_ADDRESS                            *SrcIp,      OPTIONAL
        IN OUT EFI_PXE_BASE_CODE_UDP_PORT            *SrcPort,    OPTIONAL
        IN UINTN                                     *HeaderSize, OPTIONAL
        IN VOID                                      *HeaderPtr,  OPTIONAL
        IN UINTN                                     *BufferSize,
        IN VOID                                      *BufferPtr
        );

/**
  Reads a UDP packet from the network interface.
  This function reads a UDP packet from a network interface. The data contents
  are returned in (the optional HeaderPtr and) BufferPtr, and the size of the
  buffer received is returned in BufferSize. If the input BufferSize is smaller
  than the UDP packet received (less optional HeaderSize), it will be set to the
  required size, and EFI_BUFFER_TOO_SMALL will be returned. In this case, the
  contents of BufferPtr are undefined, and the packet is lost. If a UDP packet is
  successfully received, then EFI_SUCCESS will be returned, and the information
  from the UDP header will be returned in DestIp, DestPort, SrcIp, and SrcPort if
  they are not NULL.
  Depending on the values of OpFlags and the DestIp, DestPort, SrcIp, and SrcPort
  input values, different types of UDP packet receive filtering will be performed.
  The following tables summarize these receive filter operations.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  OpFlags               The UDP operation flags.
  @param  DestIp                The destination IP address.
  @param  DestPort              The destination UDP port number.
  @param  SrcIp                 The source IP address.
  @param  SrcPort               The source UDP port number.
  @param  HeaderSize            An optional field which may be set to the length of a header at
                                HeaderPtr to be prefixed to the data at BufferPtr.
  @param  HeaderPtr             If HeaderSize is not NULL, a pointer to a header to be prefixed to the
                                data at BufferPtr.
  @param  BufferSize            A pointer to the size of the data at BufferPtr.
  @param  BufferPtr             A pointer to the data to be read.

  @retval EFI_SUCCESS           The UDP Read operation was completed.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.
  @retval EFI_BUFFER_TOO_SMALL  The packet is larger than Buffer can hold.
  @retval EFI_ABORTED           The callback function aborted the UDP Read operation.
  @retval EFI_TIMEOUT           The UDP Read operation timed out.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_UDP_READ)(
        IN EFI_PXE_BASE_CODE_PROTOCOL                *This,
        IN UINT16                                    OpFlags,
        IN OUT EFI_IP_ADDRESS                        *DestIp,     OPTIONAL
        IN OUT EFI_PXE_BASE_CODE_UDP_PORT            *DestPort,   OPTIONAL
        IN OUT EFI_IP_ADDRESS                        *SrcIp,      OPTIONAL
        IN OUT EFI_PXE_BASE_CODE_UDP_PORT            *SrcPort,    OPTIONAL
        IN UINTN                                     *HeaderSize, OPTIONAL
        IN VOID                                      *HeaderPtr,  OPTIONAL
        IN OUT UINTN                                 *BufferSize,
        IN VOID                                      *BufferPtr
        );

/**
  Updates the IP receive filters of a network device and enables software filtering.

  The NewFilter field is used to modify the network device's current IP receive
  filter settings and to enable a software filter. This function updates the IpFilter
  field of the EFI_PXE_BASE_CODE_MODE structure with the contents of NewIpFilter.
  The software filter is used when the USE_FILTER in OpFlags is set to UdpRead().
  The current hardware filter remains in effect no matter what the settings of OpFlags
  are, so that the meaning of ANY_DEST_IP set in OpFlags to UdpRead() is from those
  packets whose reception is enabled in hardware - physical NIC address (unicast),
  broadcast address, logical address or addresses (multicast), or all (promiscuous).
  UdpRead() does not modify the IP filter settings.
  Dhcp(), Discover(), and Mtftp() set the IP filter, and return with the IP receive
  filter list emptied and the filter set to EFI_PXE_BASE_CODE_IP_FILTER_STATION_IP.
  If an application or driver wishes to preserve the IP receive filter settings,
  it will have to preserve the IP receive filter settings before these calls, and
  use SetIpFilter() to restore them after the calls. If incompatible filtering is
  requested (for example, PROMISCUOUS with anything else), or if the device does not
  support a requested filter setting and it cannot be accommodated in software
  (for example, PROMISCUOUS not supported), EFI_INVALID_PARAMETER will be returned.
  The IPlist field is used to enable IPs other than the StationIP. They may be
  multicast or unicast. If IPcnt is set as well as EFI_PXE_BASE_CODE_IP_FILTER_STATION_IP,
  then both the StationIP and the IPs from the IPlist will be used.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  NewFilter             The pointer to the new set of IP receive filters.

  @retval EFI_SUCCESS           The IP receive filter settings were updated.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_SET_IP_FILTER)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN EFI_PXE_BASE_CODE_IP_FILTER           *NewFilter
        );

/**
  Uses the ARP protocol to resolve a MAC address.

  This function uses the ARP protocol to resolve a MAC address. The UsingIpv6 field
  of the EFI_PXE_BASE_CODE_MODE structure is used to determine if IPv4 or IPv6
  addresses are being used. The IP address specified by IpAddr is used to resolve
  a MAC address. If the ARP protocol succeeds in resolving the specified address,
  then the ArpCacheEntries and ArpCache fields of the EFI_PXE_BASE_CODE_MODE structure
  are updated, and EFI_SUCCESS is returned. If MacAddr is not NULL, the resolved
  MAC address is placed there as well.
  If the PXE Base Code protocol is in the stopped state, then EFI_NOT_STARTED is
  returned. If the ARP protocol encounters a timeout condition while attempting
  to resolve an address, then EFI_TIMEOUT is returned. If the Callback Protocol
  does not return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE, then EFI_ABORTED is
  returned.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  IpAddr                The pointer to the IP address that is used to resolve a MAC address.
  @param  MacAddr               If not NULL, a pointer to the MAC address that was resolved with the
                                ARP protocol.

  @retval EFI_SUCCESS           The IP or MAC address was resolved.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_DEVICE_ERROR      The network device encountered an error during this operation.
  @retval EFI_ABORTED           The callback function aborted the ARP Protocol.
  @retval EFI_TIMEOUT           The ARP Protocol encountered a timeout condition.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_ARP)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN EFI_IP_ADDRESS                        *IpAddr,
        IN EFI_MAC_ADDRESS                       *MacAddr OPTIONAL
        );

/**
  Updates the parameters that affect the operation of the PXE Base Code Protocol.

  This function sets parameters that affect the operation of the PXE Base Code Protocol.
  The parameter specified by NewAutoArp is used to control the generation of ARP
  protocol packets. If NewAutoArp is TRUE, then ARP Protocol packets will be generated
  as required by the PXE Base Code Protocol. If NewAutoArp is FALSE, then no ARP
  Protocol packets will be generated. In this case, the only mappings that are
  available are those stored in the ArpCache of the EFI_PXE_BASE_CODE_MODE structure.
  If there are not enough mappings in the ArpCache to perform a PXE Base Code Protocol
  service, then the service will fail. This function updates the AutoArp field of
  the EFI_PXE_BASE_CODE_MODE structure to NewAutoArp.
  The SetParameters() call must be invoked after a Callback Protocol is installed
  to enable the use of callbacks.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  NewAutoArp            If not NULL, a pointer to a value that specifies whether to replace the
                                current value of AutoARP.
  @param  NewSendGUID           If not NULL, a pointer to a value that specifies whether to replace the
                                current value of SendGUID.
  @param  NewTTL                If not NULL, a pointer to be used in place of the current value of TTL,
                                the "time to live" field of the IP header.
  @param  NewToS                If not NULL, a pointer to be used in place of the current value of ToS,
                                the "type of service" field of the IP header.
  @param  NewMakeCallback       If not NULL, a pointer to a value that specifies whether to replace the
                                current value of the MakeCallback field of the Mode structure.

  @retval EFI_SUCCESS           The new parameters values were updated.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_SET_PARAMETERS)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN BOOLEAN                               *NewAutoArp,     OPTIONAL
        IN BOOLEAN                               *NewSendGUID,    OPTIONAL
        IN UINT8                                 *NewTTL,         OPTIONAL
        IN UINT8                                 *NewToS,         OPTIONAL
        IN BOOLEAN                               *NewMakeCallback OPTIONAL
        );

/**
  Updates the station IP address and/or subnet mask values of a network device.

  This function updates the station IP address and/or subnet mask values of a network
  device.
  The NewStationIp field is used to modify the network device's current IP address.
  If NewStationIP is NULL, then the current IP address will not be modified. Otherwise,
  this function updates the StationIp field of the EFI_PXE_BASE_CODE_MODE structure
  with NewStationIp.
  The NewSubnetMask field is used to modify the network device's current subnet
  mask. If NewSubnetMask is NULL, then the current subnet mask will not be modified.
  Otherwise, this function updates the SubnetMask field of the EFI_PXE_BASE_CODE_MODE
  structure with NewSubnetMask.

  @param  This                  The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  NewStationIp          The pointer to the new IP address to be used by the network device.
  @param  NewSubnetMask         The pointer to the new subnet mask to be used by the network device.

  @retval EFI_SUCCESS           The new station IP address and/or subnet mask were updated.
  @retval EFI_NOT_STARTED       The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_SET_STATION_IP)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        IN EFI_IP_ADDRESS                        *NewStationIp,   OPTIONAL
        IN EFI_IP_ADDRESS                        *NewSubnetMask   OPTIONAL
        );

/**
  Updates the contents of the cached DHCP and Discover packets.

  The pointers to the new packets are used to update the contents of the cached
  packets in the EFI_PXE_BASE_CODE_MODE structure.

  @param  This                   The pointer to the EFI_PXE_BASE_CODE_PROTOCOL instance.
  @param  NewDhcpDiscoverValid   The pointer to a value that will replace the current
                                 DhcpDiscoverValid field.
  @param  NewDhcpAckReceived     The pointer to a value that will replace the current
                                 DhcpAckReceived field.
  @param  NewProxyOfferReceived  The pointer to a value that will replace the current
                                 ProxyOfferReceived field.
  @param  NewPxeDiscoverValid    The pointer to a value that will replace the current
                                 ProxyOfferReceived field.
  @param  NewPxeReplyReceived    The pointer to a value that will replace the current
                                 PxeReplyReceived field.
  @param  NewPxeBisReplyReceived The pointer to a value that will replace the current
                                 PxeBisReplyReceived field.
  @param  NewDhcpDiscover        The pointer to the new cached DHCP Discover packet contents.
  @param  NewDhcpAck             The pointer to the new cached DHCP Ack packet contents.
  @param  NewProxyOffer          The pointer to the new cached Proxy Offer packet contents.
  @param  NewPxeDiscover         The pointer to the new cached PXE Discover packet contents.
  @param  NewPxeReply            The pointer to the new cached PXE Reply packet contents.
  @param  NewPxeBisReply         The pointer to the new cached PXE BIS Reply packet contents.

  @retval EFI_SUCCESS            The cached packet contents were updated.
  @retval EFI_NOT_STARTED        The PXE Base Code Protocol is in the stopped state.
  @retval EFI_INVALID_PARAMETER  This is NULL or not point to a valid EFI_PXE_BASE_CODE_PROTOCOL structure.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PXE_BASE_CODE_SET_PACKETS)(
        IN EFI_PXE_BASE_CODE_PROTOCOL            *This,
        BOOLEAN                                  *NewDhcpDiscoverValid,   OPTIONAL
        BOOLEAN                                  *NewDhcpAckReceived,     OPTIONAL
        BOOLEAN                                  *NewProxyOfferReceived,  OPTIONAL
        BOOLEAN                                  *NewPxeDiscoverValid,    OPTIONAL
        BOOLEAN                                  *NewPxeReplyReceived,    OPTIONAL
        BOOLEAN                                  *NewPxeBisReplyReceived, OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewDhcpDiscover,        OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewDhcpAck,             OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewProxyOffer,          OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewPxeDiscover,         OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewPxeReply,            OPTIONAL
        IN EFI_PXE_BASE_CODE_PACKET              *NewPxeBisReply          OPTIONAL
        );

//
// PXE Base Code Protocol structure
//
#define EFI_PXE_BASE_CODE_PROTOCOL_REVISION   0x00010000

//
// Revision defined in EFI1.1
//
#define EFI_PXE_BASE_CODE_INTERFACE_REVISION  EFI_PXE_BASE_CODE_PROTOCOL_REVISION

///
/// The EFI_PXE_BASE_CODE_PROTOCOL is used to control PXE-compatible devices.
/// An EFI_PXE_BASE_CODE_PROTOCOL will be layered on top of an
/// EFI_MANAGED_NETWORK_PROTOCOL protocol in order to perform packet level transactions.
/// The EFI_PXE_BASE_CODE_PROTOCOL handle also supports the
/// EFI_LOAD_FILE_PROTOCOL protocol. This provides a clean way to obtain control from the
/// boot manager if the boot path is from the remote device.
///
struct _EFI_PXE_BASE_CODE_PROTOCOL {
    ///
    ///  The revision of the EFI_PXE_BASE_CODE_PROTOCOL. All future revisions must
    ///  be backwards compatible. If a future version is not backwards compatible
    ///  it is not the same GUID.
    ///
    UINT64                            Revision;
    EFI_PXE_BASE_CODE_START           Start;
    EFI_PXE_BASE_CODE_STOP            Stop;
    EFI_PXE_BASE_CODE_DHCP            Dhcp;
    EFI_PXE_BASE_CODE_DISCOVER        Discover;
    EFI_PXE_BASE_CODE_MTFTP           Mtftp;
    EFI_PXE_BASE_CODE_UDP_WRITE       UdpWrite;
    EFI_PXE_BASE_CODE_UDP_READ        UdpRead;
    EFI_PXE_BASE_CODE_SET_IP_FILTER   SetIpFilter;
    EFI_PXE_BASE_CODE_ARP             Arp;
    EFI_PXE_BASE_CODE_SET_PARAMETERS  SetParameters;
    EFI_PXE_BASE_CODE_SET_STATION_IP  SetStationIp;
    EFI_PXE_BASE_CODE_SET_PACKETS     SetPackets;
    ///
    /// The pointer to the EFI_PXE_BASE_CODE_MODE data for this device.
    ///
    EFI_PXE_BASE_CODE_MODE            *Mode;
};

//
// EFI MTFTP4

#define EFI_MTFTP4_SERVICE_BINDING_PROTOCOL_GUID \
{ \
    0x2FE800BE, 0x8F01, 0x4aa6, { \
        0x94, 0x6B, 0xD7, 0x13, 0x88, 0xE1, 0x83, 0x3F \
    } \
}

#define EFI_MTFTP4_PROTOCOL_GUID \
{ \
    0x78247c57, 0x63db, 0x4708, { \
        0x99, 0xc2, 0xa8, 0xb4, 0xa9, 0xa6, 0x1f, 0x6b \
    } \
}

typedef struct _EFI_MTFTP4_PROTOCOL EFI_MTFTP4_PROTOCOL;
typedef struct _EFI_MTFTP4_TOKEN EFI_MTFTP4_TOKEN;

//
//MTFTP4 packet opcode definition
//
#define EFI_MTFTP4_OPCODE_RRQ                     1
#define EFI_MTFTP4_OPCODE_WRQ                     2
#define EFI_MTFTP4_OPCODE_DATA                    3
#define EFI_MTFTP4_OPCODE_ACK                     4
#define EFI_MTFTP4_OPCODE_ERROR                   5
#define EFI_MTFTP4_OPCODE_OACK                    6
#define EFI_MTFTP4_OPCODE_DIR                     7
#define EFI_MTFTP4_OPCODE_DATA8                   8
#define EFI_MTFTP4_OPCODE_ACK8                    9

//
// MTFTP4 error code definition
//
#define EFI_MTFTP4_ERRORCODE_NOT_DEFINED          0
#define EFI_MTFTP4_ERRORCODE_FILE_NOT_FOUND       1
#define EFI_MTFTP4_ERRORCODE_ACCESS_VIOLATION     2
#define EFI_MTFTP4_ERRORCODE_DISK_FULL            3
#define EFI_MTFTP4_ERRORCODE_ILLEGAL_OPERATION    4
#define EFI_MTFTP4_ERRORCODE_UNKNOWN_TRANSFER_ID  5
#define EFI_MTFTP4_ERRORCODE_FILE_ALREADY_EXISTS  6
#define EFI_MTFTP4_ERRORCODE_NO_SUCH_USER         7
#define EFI_MTFTP4_ERRORCODE_REQUEST_DENIED       8

//
// MTFTP4 pacekt definitions
//
#pragma pack(1)

typedef struct {
    UINT16                  OpCode;
    UINT8                   Filename[1];
} _ms_struct _packed EFI_MTFTP4_REQ_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT8                   Data[1];
} _ms_struct _packed EFI_MTFTP4_OACK_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT16                  Block;
    UINT8                   Data[1];
} _ms_struct _packed EFI_MTFTP4_DATA_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT16                  Block[1];
} _ms_struct _packed EFI_MTFTP4_ACK_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT64                  Block;
    UINT8                   Data[1];
} _ms_struct _packed EFI_MTFTP4_DATA8_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT64                  Block[1];
} _ms_struct _packed EFI_MTFTP4_ACK8_HEADER;

typedef struct {
    UINT16                  OpCode;
    UINT16                  ErrorCode;
    UINT8                   ErrorMessage[1];
} _ms_struct _packed EFI_MTFTP4_ERROR_HEADER;

typedef union {
    ///
    /// Type of packets as defined by the MTFTPv4 packet opcodes.
    ///
    UINT16                  OpCode;
    ///
    /// Read request packet header.
    ///
    EFI_MTFTP4_REQ_HEADER   Rrq;
    ///
    /// Write request packet header.
    ///
    EFI_MTFTP4_REQ_HEADER   Wrq;
    ///
    /// Option acknowledge packet header.
    ///
    EFI_MTFTP4_OACK_HEADER  Oack;
    ///
    /// Data packet header.
    ///
    EFI_MTFTP4_DATA_HEADER  Data;
    ///
    /// Acknowledgement packet header.
    ///
    EFI_MTFTP4_ACK_HEADER   Ack;
    ///
    /// Data packet header with big block number.
    ///
    EFI_MTFTP4_DATA8_HEADER Data8;
    ///
    /// Acknowledgement header with big block num.
    ///
    EFI_MTFTP4_ACK8_HEADER  Ack8;
    ///
    /// Error packet header.
    ///
    EFI_MTFTP4_ERROR_HEADER Error;
} _ms_struct _packed EFI_MTFTP4_PACKET;

#pragma pack()

///
/// MTFTP4 option definition.
///
typedef struct {
    UINT8                   *OptionStr;
    UINT8                   *ValueStr;
} EFI_MTFTP4_OPTION;


typedef struct {
    BOOLEAN                 UseDefaultSetting;
    EFI_IPv4_ADDRESS        StationIp;
    EFI_IPv4_ADDRESS        SubnetMask;
    UINT16                  LocalPort;
    EFI_IPv4_ADDRESS        GatewayIp;
    EFI_IPv4_ADDRESS        ServerIp;
    UINT16                  InitialServerPort;
    UINT16                  TryCount;
    UINT16                  TimeoutValue;
} _packed EFI_MTFTP4_CONFIG_DATA;


typedef struct {
    EFI_MTFTP4_CONFIG_DATA  ConfigData;
    UINT8                   SupportedOptionCount;
    UINT8                   **SupportedOptions;
    UINT8                   UnsupportedOptionCount;
    UINT8                   **UnsupportedOptoins;
} EFI_MTFTP4_MODE_DATA;


typedef struct {
    EFI_IPv4_ADDRESS        GatewayIp;
    EFI_IPv4_ADDRESS        ServerIp;
    UINT16                  ServerPort;
    UINT16                  TryCount;
    UINT16                  TimeoutValue;
} EFI_MTFTP4_OVERRIDE_DATA;

//
// Protocol interfaces definition
//

/**
  A callback function that is provided by the caller to intercept
  the EFI_MTFTP4_OPCODE_DATA or EFI_MTFTP4_OPCODE_DATA8 packets processed in the
  EFI_MTFTP4_PROTOCOL.ReadFile() function, and alternatively to intercept
  EFI_MTFTP4_OPCODE_OACK or EFI_MTFTP4_OPCODE_ERROR packets during a call to
  EFI_MTFTP4_PROTOCOL.ReadFile(), WriteFile() or ReadDirectory().
  @param  This        The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token       The token that the caller provided in the
                      EFI_MTFTP4_PROTOCOL.ReadFile(), WriteFile()
                      or ReadDirectory() function.
  @param  PacketLen   Indicates the length of the packet.
  @param  Packet      The pointer to an MTFTPv4 packet.
  @retval EFI_SUCCESS The operation was successful.
  @retval Others      Aborts the transfer process.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_CHECK_PACKET)(
        IN EFI_MTFTP4_PROTOCOL  *This,
        IN EFI_MTFTP4_TOKEN     *Token,
        IN UINT16               PacketLen,
        IN EFI_MTFTP4_PACKET    *Paket
        );

/**
  Timeout callback funtion.
  @param  This           The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token          The token that is provided in the
                         EFI_MTFTP4_PROTOCOL.ReadFile() or
                         EFI_MTFTP4_PROTOCOL.WriteFile() or
                         EFI_MTFTP4_PROTOCOL.ReadDirectory() functions
                         by the caller.
  @retval EFI_SUCCESS   The operation was successful.
  @retval Others        Aborts download process.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_TIMEOUT_CALLBACK)(
        IN EFI_MTFTP4_PROTOCOL  *This,
        IN EFI_MTFTP4_TOKEN     *Token
        );

/**
  A callback function that the caller provides to feed data to the
  EFI_MTFTP4_PROTOCOL.WriteFile() function.
  @param  This   The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token  The token provided in the
                 EFI_MTFTP4_PROTOCOL.WriteFile() by the caller.
  @param  Length Indicates the length of the raw data wanted on input, and the
                 length the data available on output.
  @param  Buffer The pointer to the buffer where the data is stored.
  @retval EFI_SUCCESS The operation was successful.
  @retval Others      Aborts session.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_PACKET_NEEDED)(
        IN  EFI_MTFTP4_PROTOCOL *This,
        IN  EFI_MTFTP4_TOKEN    *Token,
        IN  OUT UINT16          *Length,
        OUT VOID                **Buffer
        );


/**
  Submits an asynchronous interrupt transfer to an interrupt endpoint of a USB device.
  @param  This     The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  ModeData The pointer to storage for the EFI MTFTPv4 Protocol driver mode data.
  @retval EFI_SUCCESS           The configuration data was successfully returned.
  @retval EFI_OUT_OF_RESOURCES  The required mode data could not be allocated.
  @retval EFI_INVALID_PARAMETER This is NULL or ModeData is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_GET_MODE_DATA)(
        IN  EFI_MTFTP4_PROTOCOL     *This,
        OUT EFI_MTFTP4_MODE_DATA    *ModeData
        );


/**
  Initializes, changes, or resets the default operational setting for this
  EFI MTFTPv4 Protocol driver instance.
  @param  This            The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  MtftpConfigData The pointer to the configuration data structure.
  @retval EFI_SUCCESS           The EFI MTFTPv4 Protocol driver was configured successfully.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_ACCESS_DENIED     The EFI configuration could not be changed at this time because
                                there is one MTFTP background operation in progress.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) has not finished yet.
  @retval EFI_UNSUPPORTED       A configuration protocol (DHCP, BOOTP, RARP, etc.) could not
                                be located when clients choose to use the default address
                                settings.
  @retval EFI_OUT_OF_RESOURCES  The EFI MTFTPv4 Protocol driver instance data could not be
                                allocated.
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred. The EFI
                                 MTFTPv4 Protocol driver instance is not configured.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_CONFIGURE)(
        IN EFI_MTFTP4_PROTOCOL       *This,
        IN EFI_MTFTP4_CONFIG_DATA    *MtftpConfigData OPTIONAL
        );


/**
  Gets information about a file from an MTFTPv4 server.
  @param  This         The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  OverrideData Data that is used to override the existing parameters. If NULL,
                       the default parameters that were set in the
                       EFI_MTFTP4_PROTOCOL.Configure() function are used.
  @param  Filename     The pointer to null-terminated ASCII file name string.
  @param  ModeStr      The pointer to null-terminated ASCII mode string. If NULL, "octet" will be used.
  @param  OptionCount  Number of option/value string pairs in OptionList.
  @param  OptionList   The pointer to array of option/value string pairs. Ignored if
                       OptionCount is zero.
  @param  PacketLength The number of bytes in the returned packet.
  @param  Packet       The pointer to the received packet. This buffer must be freed by
                       the caller.
  @retval EFI_SUCCESS              An MTFTPv4 OACK packet was received and is in the Packet.
  @retval EFI_INVALID_PARAMETER    One or more of the following conditions is TRUE:
                                   - This is NULL.
                                   - Filename is NULL.
                                   - OptionCount is not zero and OptionList is NULL.
                                   - One or more options in OptionList have wrong format.
                                   - PacketLength is NULL.
                                   - One or more IPv4 addresses in OverrideData are not valid
                                     unicast IPv4 addresses if OverrideData is not NULL.
  @retval EFI_UNSUPPORTED          One or more options in the OptionList are in the
                                   unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED          The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING           When using a default address, configuration (DHCP, BOOTP,
                                   RARP, etc.) has not finished yet.
  @retval EFI_ACCESS_DENIED        The previous operation has not completed yet.
  @retval EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval EFI_TFTP_ERROR           An MTFTPv4 ERROR packet was received and is in the Packet.
  @retval EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received and the Packet is set to NULL.
  @retval EFI_ICMP_ERROR           Some other ICMP ERROR packet was received and is in the Buffer.
  @retval EFI_PROTOCOL_ERROR       An unexpected MTFTPv4 packet was received and is in the Packet.
  @retval EFI_TIMEOUT              No responses were received from the MTFTPv4 server.
  @retval EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval EFI_NO_MEDIA             There was a media error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_GET_INFO)(
        IN  EFI_MTFTP4_PROTOCOL      *This,
        IN  EFI_MTFTP4_OVERRIDE_DATA *OverrideData   OPTIONAL,
        IN  UINT8                    *Filename,
        IN  UINT8                    *ModeStr        OPTIONAL,
        IN  UINT8                    OptionCount,
        IN  EFI_MTFTP4_OPTION        *OptionList,
        OUT UINT32                   *PacketLength,
        OUT EFI_MTFTP4_PACKET        **Packet        OPTIONAL
        );

/**
  Parses the options in an MTFTPv4 OACK packet.
  @param  This         The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  PacketLen    Length of the OACK packet to be parsed.
  @param  Packet       The pointer to the OACK packet to be parsed.
  @param  OptionCount  The pointer to the number of options in following OptionList.
  @param  OptionList   The pointer to EFI_MTFTP4_OPTION storage. Call the EFI Boot
                       Service FreePool() to release the OptionList if the options
                       in this OptionList are not needed any more.
  @retval EFI_SUCCESS           The OACK packet was valid and the OptionCount and
                                OptionList parameters have been updated.
  @retval EFI_INVALID_PARAMETER One or more of the following conditions is TRUE:
                                - PacketLen is 0.
                                - Packet is NULL or Packet is not a valid MTFTPv4 packet.
                                - OptionCount is NULL.
  @retval EFI_NOT_FOUND         No options were found in the OACK packet.
  @retval EFI_OUT_OF_RESOURCES  Storage for the OptionList array cannot be allocated.
  @retval EFI_PROTOCOL_ERROR    One or more of the option fields is invalid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_PARSE_OPTIONS)(
        IN  EFI_MTFTP4_PROTOCOL      *This,
        IN  UINT32                   PacketLen,
        IN  EFI_MTFTP4_PACKET        *Packet,
        OUT UINT32                   *OptionCount,
        OUT EFI_MTFTP4_OPTION        **OptionList OPTIONAL
        );


/**
  Downloads a file from an MTFTPv4 server.
  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.
  @retval EFI_SUCCESS              The data file has been transferred successfully.
  @retval EFI_OUT_OF_RESOURCES     Required system resources could not be allocated.
  @retval EFI_BUFFER_TOO_SMALL     BufferSize is not zero but not large enough to hold the
                                   downloaded data in downloading process.
  @retval EFI_ABORTED              Current operation is aborted by user.
  @retval EFI_NETWORK_UNREACHABLE  An ICMP network unreachable error packet was received.
  @retval EFI_HOST_UNREACHABLE     An ICMP host unreachable error packet was received.
  @retval EFI_PROTOCOL_UNREACHABLE An ICMP protocol unreachable error packet was received.
  @retval EFI_PORT_UNREACHABLE     An ICMP port unreachable error packet was received.
  @retval EFI_ICMP_ERROR           Some other  ICMP ERROR packet was received.
  @retval EFI_TIMEOUT              No responses were received from the MTFTPv4 server.
  @retval EFI_TFTP_ERROR           An MTFTPv4 ERROR packet was received.
  @retval EFI_DEVICE_ERROR         An unexpected network error or system error occurred.
  @retval EFI_NO_MEDIA             There was a media error.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_READ_FILE)(
        IN EFI_MTFTP4_PROTOCOL       *This,
        IN EFI_MTFTP4_TOKEN          *Token
        );



/**
  Sends a file to an MTFTPv4 server.
  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.
  @retval EFI_SUCCESS           The upload session has started.
  @retval EFI_UNSUPPORTED       The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       One or more options in the Token.OptionList are in
                                the unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED       The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) is not finished yet.
  @retval EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv4 session.
  @retval EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval EFI_DEVICE_ERROR      An unexpected network error or system error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_WRITE_FILE)(
        IN EFI_MTFTP4_PROTOCOL       *This,
        IN EFI_MTFTP4_TOKEN          *Token
        );


/**
  Downloads a data file "directory" from an MTFTPv4 server. May be unsupported in some EFI
  implementations.
  @param  This  The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @param  Token The pointer to the token structure to provide the parameters that are
                used in this operation.
  @retval EFI_SUCCESS           The MTFTPv4 related file "directory" has been downloaded.
  @retval EFI_UNSUPPORTED       The operation is not supported by this implementation.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       One or more options in the Token.OptionList are in
                                the unsupported list of structure EFI_MTFTP4_MODE_DATA.
  @retval EFI_NOT_STARTED       The EFI MTFTPv4 Protocol driver has not been started.
  @retval EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                RARP, etc.) is not finished yet.
  @retval EFI_ALREADY_STARTED   This Token is already being used in another MTFTPv4 session.
  @retval EFI_OUT_OF_RESOURCES  Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED     The previous operation has not completed yet.
  @retval EFI_DEVICE_ERROR      An unexpected network error or system error occurred.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_READ_DIRECTORY)(
        IN EFI_MTFTP4_PROTOCOL       *This,
        IN EFI_MTFTP4_TOKEN          *Token
        );

/**
  Polls for incoming data packets and processes outgoing data packets.
  @param  This The pointer to the EFI_MTFTP4_PROTOCOL instance.
  @retval  EFI_SUCCESS           Incoming or outgoing data was processed.
  @retval  EFI_NOT_STARTED       This EFI MTFTPv4 Protocol instance has not been started.
  @retval  EFI_NO_MAPPING        When using a default address, configuration (DHCP, BOOTP,
                                 RARP, etc.) is not finished yet.
  @retval  EFI_INVALID_PARAMETER This is NULL.
  @retval  EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval  EFI_TIMEOUT           Data was dropped out of the transmit and/or receive queue.
                                 Consider increasing the polling rate.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MTFTP4_POLL)(
        IN EFI_MTFTP4_PROTOCOL       *This
        );

///
/// The EFI_MTFTP4_PROTOCOL is designed to be used by UEFI drivers and applications
/// to transmit and receive data files. The EFI MTFTPv4 Protocol driver uses
/// the underlying EFI UDPv4 Protocol driver and EFI IPv4 Protocol driver.
///
struct _EFI_MTFTP4_PROTOCOL {
    EFI_MTFTP4_GET_MODE_DATA     GetModeData;
    EFI_MTFTP4_CONFIGURE         Configure;
    EFI_MTFTP4_GET_INFO          GetInfo;
    EFI_MTFTP4_PARSE_OPTIONS     ParseOptions;
    EFI_MTFTP4_READ_FILE         ReadFile;
    EFI_MTFTP4_WRITE_FILE        WriteFile;
    EFI_MTFTP4_READ_DIRECTORY    ReadDirectory;
    EFI_MTFTP4_POLL              Poll;
};

struct _EFI_MTFTP4_TOKEN {
    ///
    /// The status that is returned to the caller at the end of the operation
    /// to indicate whether this operation completed successfully.
    ///
    EFI_STATUS                  Status;
    ///
    /// The event that will be signaled when the operation completes. If
    /// set to NULL, the corresponding function will wait until the read or
    /// write operation finishes. The type of Event must be
    /// EVT_NOTIFY_SIGNAL. The Task Priority Level (TPL) of
    /// Event must be lower than or equal to TPL_CALLBACK.
    ///
    EFI_EVENT                   Event;
    ///
    /// If not NULL, the data that will be used to override the existing configure data.
    ///
    EFI_MTFTP4_OVERRIDE_DATA    *OverrideData;
    ///
    /// The pointer to the null-terminated ASCII file name string.
    ///
    UINT8                       *Filename;
    ///
    /// The pointer to the null-terminated ASCII mode string. If NULL, "octet" is used.
    ///
    UINT8                       *ModeStr;
    ///
    /// Number of option/value string pairs.
    ///
    UINT32                      OptionCount;
    ///
    /// The pointer to an array of option/value string pairs. Ignored if OptionCount is zero.
    ///
    EFI_MTFTP4_OPTION           *OptionList;
    ///
    /// The size of the data buffer.
    ///
    UINT64                      BufferSize;
    ///
    /// The pointer to the data buffer. Data that is downloaded from the
    /// MTFTPv4 server is stored here. Data that is uploaded to the
    /// MTFTPv4 server is read from here. Ignored if BufferSize is zero.
    ///
    VOID                        *Buffer;
    ///
    /// The pointer to the context that will be used by CheckPacket,
    /// TimeoutCallback and PacketNeeded.
    ///
    VOID                        *Context;
    ///
    /// The pointer to the callback function to check the contents of the received packet.
    ///
    EFI_MTFTP4_CHECK_PACKET     CheckPacket;
    ///
    /// The pointer to the function to be called when a timeout occurs.
    ///
    EFI_MTFTP4_TIMEOUT_CALLBACK TimeoutCallback;
    ///
    /// The pointer to the function to provide the needed packet contents.
    ///
    EFI_MTFTP4_PACKET_NEEDED    PacketNeeded;
};

#define EFI_SIMPLE_POINTER_PROTOCOL_GUID \
  { \
    0x31878c87, 0xb75, 0x11d5, {0x9a, 0x4f, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

typedef struct _EFI_SIMPLE_POINTER_PROTOCOL  EFI_SIMPLE_POINTER_PROTOCOL;

//
// Data structures
//
typedef struct {
  ///
  /// The signed distance in counts that the pointer device has been moved along the x-axis.
  ///
  INT32   RelativeMovementX;
  ///
  /// The signed distance in counts that the pointer device has been moved along the y-axis.
  ///
  INT32   RelativeMovementY;
  ///
  /// The signed distance in counts that the pointer device has been moved along the z-axis.
  ///
  INT32   RelativeMovementZ;
  ///
  /// If TRUE, then the left button of the pointer device is being
  /// pressed. If FALSE, then the left button of the pointer device is not being pressed.
  ///
  BOOLEAN LeftButton;
  ///
  /// If TRUE, then the right button of the pointer device is being
  /// pressed. If FALSE, then the right button of the pointer device is not being pressed.
  ///
  BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_STATE;

typedef struct {
  ///
  /// The resolution of the pointer device on the x-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionX;
  ///
  /// The resolution of the pointer device on the y-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionY;
  ///
  /// The resolution of the pointer device on the z-axis in counts/mm.
  /// If 0, then the pointer device does not support an x-axis.
  ///
  UINT64  ResolutionZ;
  ///
  /// TRUE if a left button is present on the pointer device. Otherwise FALSE.
  ///
  BOOLEAN LeftButton;
  ///
  /// TRUE if a right button is present on the pointer device. Otherwise FALSE.
  ///
  BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_MODE;

/**
  Resets the pointer device hardware.
  @param  This                  A pointer to the EFI_SIMPLE_POINTER_PROTOCOL
                                instance.
  @param  ExtendedVerification  Indicates that the driver may perform a more exhaustive
                                verification operation of the device during reset.
  @retval EFI_SUCCESS           The device was reset.
  @retval EFI_DEVICE_ERROR      The device is not functioning correctly and could not be reset.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_POINTER_RESET)(
  IN EFI_SIMPLE_POINTER_PROTOCOL            *This,
  IN BOOLEAN                                ExtendedVerification
  );

/**
  Retrieves the current state of a pointer device.
  @param  This                  A pointer to the EFI_SIMPLE_POINTER_PROTOCOL
                                instance.
  @param  State                 A pointer to the state information on the pointer device.
  @retval EFI_SUCCESS           The state of the pointer device was returned in State.
  @retval EFI_NOT_READY         The state of the pointer device has not changed since the last call to
                                GetState().
  @retval EFI_DEVICE_ERROR      A device error occurred while attempting to retrieve the pointer device's
                                current state.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_POINTER_GET_STATE)(
  IN EFI_SIMPLE_POINTER_PROTOCOL          *This,
  IN OUT EFI_SIMPLE_POINTER_STATE         *State
  );

///
/// The EFI_SIMPLE_POINTER_PROTOCOL provides a set of services for a pointer
/// device that can use used as an input device from an application written
/// to this specification. The services include the ability to reset the
/// pointer device, retrieve get the state of the pointer device, and
/// retrieve the capabilities of the pointer device.
///
struct _EFI_SIMPLE_POINTER_PROTOCOL {
  EFI_SIMPLE_POINTER_RESET      Reset;
  EFI_SIMPLE_POINTER_GET_STATE  GetState;
  ///
  /// Event to use with WaitForEvent() to wait for input from the pointer device.
  ///
  EFI_EVENT                     WaitForInput;
  ///
  /// Pointer to EFI_SIMPLE_POINTER_MODE data.
  ///
  EFI_SIMPLE_POINTER_MODE       *Mode;
};

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { \
        0x9042a9de, 0x23dc, 0x4a38, { \
            0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a \
        } \
    }

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
        IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        IN UINT32 ModeNumber,
        OUT UINTN *SizeOfInfo,
        OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
        IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        IN UINT32 ModeNumber);

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
        IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer OPTIONAL,
        IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
        IN UINTN SourceX, IN UINTN SourceY,
        IN UINTN DestinationX, IN UINTN DestinationY,
        IN UINTN Width, IN UINTN Height,
        IN UINTN Delta OPTIONAL);

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

extern EFI_GUID gEfiSimplePointerProtocolGuid;

extern EFI_HANDLE efi_image_handle;
extern EFI_SYSTEM_TABLE *efi_systab;
