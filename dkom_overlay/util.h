#pragma once
#include "includes.h"
#include "structs.h"

extern "C" {
	NTSTATUS 
		WINAPI ZwQuerySystemInformation(
		_In_      SYSTEM_INFORMATION_CLASS SystemInformationClass,
		_Inout_   PVOID                    SystemInformation,
		_In_      ULONG                    SystemInformationLength,
		_Out_opt_ PULONG                   ReturnLength
	);
	NTSTATUS
		MmCopyVirtualMemory(
			IN  PEPROCESS FromProcess,
			IN  CONST VOID *FromAddress,
			IN  PEPROCESS ToProcess,
			OUT PVOID ToAddress,
			IN  SIZE_T BufferSize,
			IN  KPROCESSOR_MODE PreviousMode,
			OUT PSIZE_T NumberOfBytesCopied
		);
	NTKERNELAPI
		PVOID
			NTAPI
				RtlFindExportedRoutineByName (
			_In_ PVOID ImageBase,
			_In_ PCCH RoutineNam
		);
}

namespace util
{
	struct string_t {
		static UNICODE_STRING ansi_to_unicode( const char* str ) {
			ANSI_STRING a_str{};
			UNICODE_STRING u_str{};

			RtlInitAnsiString(&a_str, str);
			RtlAnsiStringToUnicodeString(&u_str, &a_str, true);

			return u_str;
		}
	};

	struct system_t {
		template< typename t >
		static t get_routine_address( const char* routine_name ) {
			UNICODE_STRING u_str = string_t::ansi_to_unicode( routine_name );

			return ( t )MmGetSystemRoutineAddress(&u_str);
		}
	};

	struct process_t {
		static inline PEPROCESS e_process = { };

		static PEPROCESS get_e_process( std::int32_t process_id ) {
			PEPROCESS proc = { };

			if ( !NT_SUCCESS( PsLookupProcessByProcessId( reinterpret_cast< HANDLE >( process_id ), &proc ) ) )
				return nullptr;

			return proc;
		}
	};

	struct memory_t {
		static bool safe_copy( void* dst, void* src, size_t size ) {
			size_t bytes = 0; 

			if ( !NT_SUCCESS( MmCopyVirtualMemory( IoGetCurrentProcess( ), src, IoGetCurrentProcess( ), dst, size, KernelMode, &bytes ) ) )
				return false;

			return true;
		}
	};

	struct module_t {
		static inline module_t* ntos = nullptr, *win32k = nullptr, *win32k_base = nullptr;

		static module_t* get_ntos_base( ) {
			const auto idt_base = reinterpret_cast< std::uintptr_t >( KeGetPcr( )->IdtBase );
			auto align_page = *reinterpret_cast< std::uintptr_t* >( idt_base + 4 ) >> 0xc << 0xc;

			for ( ; align_page; align_page -= 0x1000 )
			{
				for ( int index = 0; index < 0x1000 - 0x7; index++ ) {
					const auto current_address = static_cast< std::intptr_t >( align_page ) + index;

					if 
						(
							*reinterpret_cast< std::uint8_t* >( current_address )       ==	0x48 &&
							*reinterpret_cast< std::uint8_t* >( current_address + 0x1 ) ==   0x8d &&
							*reinterpret_cast< std::uint8_t* >( current_address + 0x2 ) ==   0x1d &&
							*reinterpret_cast< std::uint8_t* >( current_address + 0x6 ) ==   0xff
							) 
					{
						const auto nto_base_offset = *reinterpret_cast< int* >( current_address + 0x3 );
						const auto nto_base_ = ( current_address + nto_base_offset + 0x7 );

						if (! ( nto_base_ & 0xfff ) )
							return reinterpret_cast< module_t* >(nto_base_);
					}
				}
			}
			return nullptr;
		}

		static module_t* get_system_module_base( const char* name ) {
			module_t* base = 0;
			ULONG bytes = 0;

			ZwQuerySystemInformation( SystemModuleInformation, 0, bytes, &bytes );

			if ( !bytes ) {
				log_e( "get_system_module_base failed...\n" );
				return nullptr;
			}

			const auto modules = reinterpret_cast< RTL_PROCESS_MODULES* >( ExAllocatePoolWithTag( NonPagedPoolNx, bytes, 'udom' ) );

			if ( !modules )
				return 0;

			if ( !NT_SUCCESS( ZwQuerySystemInformation( SystemModuleInformation, modules, bytes, &bytes ) ) ) {
				ExFreePoolWithTag( modules, 'udom' );
				return 0;
			}

			for ( ULONG i = 0; i < modules->NumberOfModules; i++ ) {
				const auto current_module = modules->Modules[ i ];

				if ( !_stricmp( reinterpret_cast< const char* >( current_module.FullPathName ), name ) ) {
					base = reinterpret_cast< module_t* >( current_module.ImageBase );
					break;
				}
			}

			if ( modules )
				ExFreePoolWithTag( modules, 'udom' );

			return base;
		}

		template< typename t >
		t get_export( char* name ) {
			return ( t )RtlFindExportedRoutineByName( this, name );
		}

		__forceinline std::uint8_t* find_pattern( module_t* _this, std::uint32_t size, const char* pattern, const char* mask ) {
			auto check_mask = [ ]( std::uint8_t* buf, const char* _pattern, const char* _mask ) -> bool {
				for ( std::uint8_t* x = buf; *_mask; _pattern++, _mask++, x++ ) {
					const auto addr = *( std::uint8_t* )( _pattern );

					if ( addr != *x && *_mask != '?' )
						return false;
				}
				return true;
			};

			for ( int i = 0; i < size - strlen( mask ); i++ ) {
				const auto addr = reinterpret_cast< std::uint8_t* >(_this) + i;

				if ( check_mask( addr, pattern, mask ) )
					return addr;
			}
			return nullptr;
		}

		__forceinline std::uint8_t* find_pattern( const char* pattern, const char* mask ) {
			const auto dos = reinterpret_cast< IMAGE_DOS_HEADER* >( this );

			const auto header = reinterpret_cast< IMAGE_NT_HEADERS64* >(reinterpret_cast< std::uintptr_t >( this ) + dos->e_lfanew);

			if ( !header ) {
				log_e( "nt header invalid\n" );
				return nullptr;
			}

			auto section = IMAGE_FIRST_SECTION( header );

			if ( !section ) {
				log_e( "pe section invalid\n" );
				return nullptr;
			}

			for ( int i = 0; i < header->FileHeader.NumberOfSections; i++, section++ ) {
				if ( !memcmp( section->Name, _( ".text" ), 5 ) || !memcmp( section->Name, _( "PAGE" ), 4 ) )
					return find_pattern( reinterpret_cast< module_t* >( this + section->VirtualAddress ), section->Misc.VirtualSize, pattern, mask );
			}
			return nullptr;
		}

		static bool init( ) {
			ntos = get_ntos_base( );

			if ( !ntos ) {
				log_e( "couldn't obtain ntos base...\n" );
				return false;
			}

			win32k = util::module_t::get_system_module_base( _( "\\SystemRoot\\System32\\win32k.sys" ) );

			if ( !win32k ) {
				log_e( "couldn't obtain win32k...\n" );
				return false;
			}

			win32k_base = util::module_t::get_system_module_base( _( "\\SystemRoot\\System32\\win32kbase.sys" ) );

			if ( !win32k_base ) {
				log_e( "couldn't obtain win32kbase...\n" );
				return false;
			}

			log_s( "initialized modules!\n" );

			return true;
		}
	};
}