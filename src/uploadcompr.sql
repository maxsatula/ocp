DECLARE
	f_handle UTL_FILE.FILE_TYPE;
	c_handle BINARY_INTEGER;
	raw_buffer RAW(32767);
	pos NUMBER;
	rindex BINARY_INTEGER;
	slno BINARY_INTEGER;
	target NUMBER;
BEGIN
	rindex := dbms_application_info.set_session_longops_nohint;
	select object_id
	  into target
	  from all_objects
	 where object_type = 'DIRECTORY'
	       and object_name = :directory;
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(:blob);
	f_handle := UTL_FILE.FOPEN(:directory, :filename, :openmode);
	pos := :skipped;
	LOOP
		BEGIN
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, 'GUNZIP', target, 0, pos, :file_size, :directory || ':' || :filename, 'bytes');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_FILE.FCLOSE(f_handle);
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);
END;
