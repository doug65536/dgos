#pragma once

/*
 * Portions Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.
 * Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
 * This source in this file are licensed and made available under the terms
 * and conditions of the BSD License. The full text of the license may be
 * found at http://opensource.org/licenses/bsd-license.php.
 */

#include "types.h"

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
  /// Allocate any available range of pages whose uppermost address is less than
  /// or equal to a specified maximum address.
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
  /// The data portions of a loaded application and the default data allocation
  /// type used by an application to allocate pool memory.
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
  /// data allocation type used by a Runtime Services Driver to allocate pool memory.
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
  /// be mapped by the OS to a virtual address so it can be accessed by EFI runtime services.
  ///
  EfiMemoryMappedIO,
  ///
  /// System memory-mapped IO region that is used to translate memory
  /// cycles to IO cycles by the processor.
  ///
  EfiMemoryMappedIOPortSpace,
  ///
  /// Address space reserved by the firmware for code that is part of the processor.
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
                                MemoryType values in the range 0x70000000..0x7FFFFFFF
                                are reserved for OEM use. MemoryType values in the range
                                0x80000000..0xFFFFFFFF are reserved for use by UEFI OS loaders
                                that are provided by operating system vendors.
  @param[in]       Pages        The number of contiguous 4 KB pages to allocate.
  @param[in, out]  Memory       The pointer to a physical address. On input, the way in which the address is
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
  @retval EFI_INVALID_PARAMETER Memory is not a page-aligned address or Pages is invalid.
  @retval EFI_NOT_FOUND         The requested memory pages were not allocated with
                                AllocatePages().
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
  /// Attributes of the memory region that describe the bit mask of capabilities
  /// for that memory region, and not necessarily the current settings for that
  /// memory region.
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
  IN     EFI_GUID                 *Protocol,
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
  IN EFI_GUID                 *Protocol,
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
  IN EFI_GUID                 *Protocol,
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
  IN  EFI_GUID                 *Protocol,
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
  IN  EFI_GUID                 *Protocol,
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
  IN     EFI_GUID                 *Protocol,    OPTIONAL
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
  IN     EFI_GUID                         *Protocol,
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
  IN EFI_GUID                 *Guid,
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
  IN  EFI_GUID                  *Protocol,
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
  IN EFI_GUID                 *Protocol,
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
  IN  EFI_GUID                            *Protocol,
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
  OUT EFI_GUID        ***ProtocolBuffer,
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
  IN     EFI_GUID                     *Protocol,      OPTIONAL
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
  IN  EFI_GUID  *Protocol,
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

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
  { \
    0x387477c2, 0x69c7, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

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
  IN CHAR16                                 *String
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
typedef UINTN RETURN_STATUS;

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
#define RETURN_ERROR(StatusCode)     (((INTN)(RETURN_STATUS)(StatusCode)) < 0)

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


/**
  Returns a 16-bit signature built from 2 ASCII characters.
  This macro returns a 16-bit value built from the two ASCII characters specified
  by A and B.
  @param  A    The first ASCII character.
  @param  B    The second ASCII character.
  @return A 16-bit value built from the two ASCII characters specified by A and B.
**/
#define SIGNATURE_16(A, B)        ((A) | (B << 8))

/**
  Returns a 32-bit signature built from 4 ASCII characters.
  This macro returns a 32-bit value built from the four ASCII characters specified
  by A, B, C, and D.
  @param  A    The first ASCII character.
  @param  B    The second ASCII character.
  @param  C    The third ASCII character.
  @param  D    The fourth ASCII character.
  @return A 32-bit value built from the two ASCII characters specified by A, B,
          C and D.
**/
#define SIGNATURE_32(A, B, C, D)  (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))

/**
  Returns a 64-bit signature built from 8 ASCII characters.
  This macro returns a 64-bit value built from the eight ASCII characters specified
  by A, B, C, D, E, F, G,and H.
  @param  A    The first ASCII character.
  @param  B    The second ASCII character.
  @param  C    The third ASCII character.
  @param  D    The fourth ASCII character.
  @param  E    The fifth ASCII character.
  @param  F    The sixth ASCII character.
  @param  G    The seventh ASCII character.
  @param  H    The eighth ASCII character.
  @return A 64-bit value built from the two ASCII characters specified by A, B,
          C, D, E, F, G and H.
**/
#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
    (SIGNATURE_32 (A, B, C, D) | ((UINT64) (SIGNATURE_32 (E, F, G, H)) << 32))

#if defined(_MSC_EXTENSIONS) && !defined (__INTEL_COMPILER) && !defined (MDE_CPU_EBC)
  #pragma intrinsic(_ReturnAddress)
  /**
    Get the return address of the calling funcation.
    Based on intrinsic function _ReturnAddress that provides the address of
    the instruction in the calling function that will be executed after
    control returns to the caller.
    @param L    Return Level.
    @return The return address of the calling funcation or 0 if L != 0.
  **/
  #define RETURN_ADDRESS(L)     ((L == 0) ? _ReturnAddress() : (VOID *) 0)
#elif defined(__GNUC__)
  void * __builtin_return_address (unsigned int level);
  /**
    Get the return address of the calling funcation.
    Based on built-in Function __builtin_return_address that returns
    the return address of the current function, or of one of its callers.
    @param L    Return Level.
    @return The return address of the calling funcation.
  **/
  #define RETURN_ADDRESS(L)     __builtin_return_address (L)
#else
  /**
    Get the return address of the calling funcation.
    @param L    Return Level.
    @return 0 as compilers don't support this feature.
  **/
  #define RETURN_ADDRESS(L)     ((VOID *) 0)
#endif

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
  IN CHAR16                   *FileName,
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
  IN EFI_GUID                 *InformationType,
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
  IN EFI_GUID                 *InformationType,
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

///
/// Revision defined in EFI1.1
///
#define EFI_FILE_IO_INTERFACE_REVISION  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
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
  IN CHAR16                   *FileName,
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
  IN EFI_GUID                 *InformationType,
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
  IN EFI_GUID                 *InformationType,
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

