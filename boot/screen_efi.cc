#include "screen.h"
#include "bootefi.h"
#include "ctors.h"
#include "likely.h"
#include "halt.h"
#include "assert.h"

tchar const boxchars[] = TSTR "╔╗║│╚╝═─█ ";

//static EFI_GUID efi_simple_text_output_protocol_guid = {
//    0x387477c2, 0x69c7, 0x11d2, {
//         0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b
//    }
//};

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *efi_simple_text_output;

_constructor(ctor_console) static void conout_init()
{
    // Just use ConOut!
    efi_simple_text_output = efi_systab->ConOut;

//    EFI_STATUS status;

//    EFI_HANDLE *efi_text_output_handles = nullptr;
//    UINTN efi_num_text_output_handles = 0;

//    status = efi_systab->BootServices->LocateHandleBuffer(
//                ByProtocol,
//                &efi_simple_text_output_protocol_guid,
//                nullptr,
//                &efi_num_text_output_handles,
//                &efi_text_output_handles);

//    if (unlikely(EFI_ERROR(status)))
//        PANIC(TSTR "Unable to query text output handle");

//    status = efi_systab->BootServices->HandleProtocol(
//                efi_text_output_handles[0],
//            &efi_simple_text_output_protocol_guid,
//            (VOID**)&efi_simple_text_output);

//    if (unlikely(EFI_ERROR(status)))
//        PANIC(TSTR "Unable to query text output interface");

//    efi_simple_text_output->SetMode(efi_simple_text_output, 0);
}

void scroll_screen(uint8_t attr)
{
    if (unlikely(!efi_simple_text_output))
        return;

    efi_simple_text_output->SetAttribute(efi_simple_text_output, attr);
    efi_simple_text_output->SetCursorPosition(efi_simple_text_output, 0, 24);
    efi_simple_text_output->OutputString(efi_simple_text_output, TSTR "\n");
}

void print_at(int col, int row, uint8_t attr,
              size_t length, tchar const *text)
{
    if (unlikely(!efi_simple_text_output))
        return;

    efi_simple_text_output->SetCursorPosition(efi_simple_text_output, col, row);
    efi_simple_text_output->SetAttribute(efi_simple_text_output, attr);
    efi_simple_text_output->OutputString(efi_simple_text_output, text);
}
