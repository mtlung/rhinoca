// Common status
roStatusEnum( undefined )
roStatusEnum( assertion )
roStatusEnum( unit_test_fail )
roStatusEnum( user_abort )
roStatusEnum( not_supported )
roStatusEnum( not_implemented )
roStatusEnum( not_enough_memory )
roStatusEnum( not_found )
roStatusEnum( invalid_parameter )
roStatusEnum( arithmetic_overflow )
roStatusEnum( division_with_zero )
roStatusEnum( numeric_cast_overflow )
roStatusEnum( pointer_is_null )
roStatusEnum( index_out_of_range )
roStatusEnum( already_exist )
roStatusEnum( not_initialized )
roStatusEnum( not_loaded )
roStatusEnum( not_available )
roStatusEnum( non_safe_assign )
roStatusEnum( cannot_be_itself )
roStatusEnum( will_cause_recursion )
roStatusEnum( timed_out )
roStatusEnum( data_corrupted )
roStatusEnum( end_of_data )
roStatusEnum( not_enough_data )
roStatusEnum( in_progress )
roStatusEnum( can_read_only )
roStatusEnum( type_mismatch )
roStatusEnum( retry_later )

// String
roStatusEnum( string_encoding_error )
roStatusEnum( string_parsing_error )
roStatusEnum( string_to_number_sign_mistmatch )
roStatusEnum( string_to_number_overflow )

// File
roStatusEnum( file_error )
roStatusEnum( file_not_open )
roStatusEnum( file_read_error )
roStatusEnum( file_write_error )
roStatusEnum( file_seek_error )

roStatusEnum( file_already_exists )
roStatusEnum( file_open_too_many_open_files )
roStatusEnum( file_open_error )

roStatusEnum( file_not_found )
roStatusEnum( file_access_denied )
roStatusEnum( file_ended )
roStatusEnum( file_error_delete )

roStatusEnum( file_is_empty )

// Network
roStatusEnum( net_error )
roStatusEnum( net_resolve_host_fail )
roStatusEnum( net_cannont_connect )
roStatusEnum( net_connection_lost )
roStatusEnum( net_not_connected )

roStatusEnum( net_already )		// EALREADY, Operation already in progress
roStatusEnum( net_connaborted )	// ECONNABORTED, Software caused connection abort
roStatusEnum( net_connreset )	// ECONNRESET, Connection reset by peer
roStatusEnum( net_connrefused )	// ECONNREFUSED, Connection refused
roStatusEnum( net_hostdown )	// EHOSTDOWN, Host is down
roStatusEnum( net_hostunreach )	// EHOSTUNREACH, No route to host
roStatusEnum( net_inprogress )	// EINPROGRESS, Operation now in progress
roStatusEnum( net_netdown )		// ENETDOWN, Network is down
roStatusEnum( net_netreset )	// ENETRESET, Network dropped connection on reset
roStatusEnum( net_nobufs )		// ENOBUFS, No buffer space available (recoverable)
roStatusEnum( net_notconn )		// ENOTCONN, Socket is not connected
roStatusEnum( net_notsock )		// ENOTSOCK, Socket operation on non-socket
roStatusEnum( net_timeout )		// ETIMEDOUT, Connection timed out
roStatusEnum( net_wouldblock )	// EWOULDBLOCK, Operation would block (recoverable)

// Http
roStatusEnum( http_error )
roStatusEnum( http_invalid_uri )
roStatusEnum( http_header_error )
roStatusEnum( http_unknow_size )
roStatusEnum( http_404_not_found )

// Json
roStatusEnum( json_parse_error )
roStatusEnum( json_missing_root_object )
roStatusEnum( json_expect_object_begin )
roStatusEnum( json_expect_object_end )
roStatusEnum( json_expect_object_name )
roStatusEnum( json_expect_array_begin )
roStatusEnum( json_expect_array_end )
roStatusEnum( json_expect_string )
roStatusEnum( json_expect_number )

// Serialization
roStatusEnum( serialization_member_mismatch )

// Image
roStatusEnum( image_invalid_header )

roStatusEnum( image_bmp_error )
roStatusEnum( image_bmp_only_24bits_color_supported )
roStatusEnum( image_bmp_compression_not_supported )

roStatusEnum( image_png_error )

roStatusEnum( image_jpeg_error )
roStatusEnum( image_jpeg_channel_count_not_supported )
