#include "includes.h"
#include "util.h"

#define rva(addr, size)((PBYTE)(addr + *(DWORD*)(addr + ((size) - 4)) + size))

// .data ptr signature 48 8B 05 FD 6F 05 ? 48 85 C0 ( NtMITPostWindowEventMessage )
std::int64_t (__fastcall* orig_callback)(void*, void*) = nullptr;

struct comms_t {
	std::uint32_t key;
	bool val;

	struct {
		void* handle;
		std::uint8_t flags;
	}window;
};

bool handle_overlay( comms_t* ptr ) {
	/*
	!! WARNING !!
	This overlay dkom method will result in a blue screen of death if you try to close the host process / destroy window without restoring the old state.
	!! WARNING !!
	*/

	if ( !ptr->window.handle ) {
		log_e( "passed hwnd invalid...\n" );
		return false;
	}

	/* 
	Exposing TAG_WND structure pointer of our hwnd
	Note: If you're calling this export from a thread without Win32StartAddress, it will not work. 
	Solutions: Attach to a process with a win32thread, dkom the win32startaddress of the target to your thread's win32startaddress.
	*/

	static const auto validate_hwnd = util::module_t::win32k_base->get_export< TAG_WND* ( * )( void* ) >( _( "ValidateHwnd" ) );

	if ( !validate_hwnd ) {
		log_e( "couldn't find validate_hwnd...\n" );
		return false;
	}

	// Validating the kernel window handle
	const auto our_wnd = validate_hwnd( ptr->window.handle );

	if ( !our_wnd ) {
		log_e( "failed to obtain our window instance...\n" );
		return false;
	}

	/* 
	This dkom method is designed to work on Windows 10, 21H2 and the offsets will be outdated on many versions.
	You might bluescreen if you don't do this check.

	Run this check on same winver Virtual Machine to validate offsets and prevent BSOD.
	This check should return log of the offsets if it finds instance->Next->Previous or instance->Previous->Next ( 2 matches ).

	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	const auto w = reinterpret_cast< std::uintptr_t >( our_wnd );
		for ( std::uint32_t i = 0; i < 0x100; i += 0x8 ) {
			if ( MmIsAddressValid( reinterpret_cast< void* >( w + i ) ) ) {
				auto w_2 = *( std::uintptr_t* )( w + i );
				for ( std::uint32_t x = 0; x < 0x100; x += 0x8 ) {
					if ( MmIsAddressValid( reinterpret_cast< void* >( w_2 + x ) ) ) {
						if ( *( std::uintptr_t* )( w_2 + x ) == w ) {
							log( "%llx | %llx\n", i, x );
						}
					}
				}
			}
		}
	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	*/

	// Sanity check to avoid bsod.
	if ( our_wnd != our_wnd->prev->next || our_wnd != our_wnd->next->prev ) {
		log_e( "TAG_WND structure outdated, check offsets with bruteforcer above!\n" );
		return false;
	}

	/* 
	This right here swaps the order of the list making it so our overlay handle will never be in the list. 
	The handle disappears and our window will be invisible from enumerations ( Check on Process Hacker / Spy++ / EnumWindows callback )
	This method also made it so that BattlEye shellcode emulator was unable to find the window and it's properties.
	*/
	if ( !( our_wnd->prev->next = our_wnd->next ) || !( our_wnd->next->prev = our_wnd->prev ) ) {
		log_e( "something is incredibly fucked...\n" );
		return false;
	}

	// Simple as that, we have achieved dkomed window, now up-to reader: you have to fix BSODs when you close the software owning the window handle. Don't add me to ask how to do it :=)
	log_s( "overlay handled successfully!\n" );

	return true;
}

std::int64_t callback( comms_t* a1, void* a2 ) {
	// for this example i cba to make communication code enum, 1 is enough for the POC.

	static comms_t buffer = { };
	if ( ExGetPreviousMode( ) != UserMode || !util::memory_t::safe_copy( &buffer, a1, sizeof( comms_t )) || buffer.key != 0xCA ) {
		log_e( "call wasn't ours...\n" );
		return orig_callback( a1, a2 );
	}

	handle_overlay( a1 );

	return 0;
}

NTSTATUS entry( ) {

	if ( !util::module_t::init( ) )
		return STATUS_UNSUCCESSFUL;

	//Forgive me for using a data pointer to communicate, the caller thread will 100% have a thread context of a win32 thread which is required to process the overlay with this method.

	const auto ptr = rva( util::module_t::win32k->find_pattern( _( "\x48\x8B\x05\xFD\x6F\x05\x00\x48\x85\xC0" ), _( "xxxxxx?xxx" ) ), 7 );

	if ( !ptr ) {
		log_e( ".data ptr not found...\n" );
		return STATUS_UNSUCCESSFUL;
	}

	if ( !( *reinterpret_cast< void** >( &orig_callback ) = _InterlockedExchangePointer( reinterpret_cast< void** >( ptr ), callback ) ) ){
		log_e( "data ptr swap failed...\n" );
		return STATUS_UNSUCCESSFUL;
	}

	log_s( "driver initialized!\n" );

	return STATUS_SUCCESS;
}