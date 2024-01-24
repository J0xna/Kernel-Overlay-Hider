#include "includes.h"
#include "util.h"

#define rva(addr, size)((PBYTE)(addr + *(DWORD*)(addr + ((size) - 4)) + size))

// .data ptr signature 48 8B 05 FD 6F 05 ? 48 85 C0 ( NtMITPostWindowEventMessage )
std::int64_t (__fastcall* orig_callback)(void*, void*) = nullptr;

struct comms_t {
	std::uint32_t key;

	struct {
		void* handle;
	}window;
};

bool handle_overlay( comms_t* ptr ) {
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

	// Sanity check to avoid Blue Screen of Death.
	if ( our_wnd != our_wnd->prev->next || our_wnd != our_wnd->next->prev ) {
		log_e( "TAG_WND structure outdated, check offsets with bruteforcer above!\n" );
		return false;
	}

	if ( !( our_wnd->prev->next = our_wnd->next ) || !( our_wnd->next->prev = our_wnd->prev ) ) {
		log_e( "something went really really wrong...\n" );
		return false;
	}

	log_s( "overlay handled successfully!\n" );

	return true;
}

std::int64_t callback( comms_t* a1, void* a2 ) {
	static comms_t buffer = { };
	if ( ExGetPreviousMode( ) != UserMode || !util::memory_t::safe_copy( &buffer, a1, sizeof( comms_t )) || buffer.key != 0xCA ) {
		return orig_callback( a1, a2 );
	}

	handle_overlay( a1 );

	return 0;
}

NTSTATUS entry( ) {

	if ( !util::module_t::init( ) )
		return STATUS_UNSUCCESSFUL;

	const auto ptr = rva( util::module_t::win32k->find_pattern( _( "\x48\x8B\x05\xFD\x6F\x05\x00\x48\x85\xC0" ), _( "xxxxxx?xxx" ) ), 7 );

	if ( !ptr ) {
		log_e( "ptr not found...\n" );
		return STATUS_UNSUCCESSFUL;
	}

	if ( !( *reinterpret_cast< void** >( &orig_callback ) = _InterlockedExchangePointer( reinterpret_cast< void** >( ptr ), callback ) ) ){
		log_e( "swapping pointer failed...\n" );
		return STATUS_UNSUCCESSFUL;
	}

	log_s( "driver initialized!\n" );

	return STATUS_SUCCESS;
}
