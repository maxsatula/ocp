DECLARE
	f_handle UTL_FILE.FILE_TYPE;
	c_handle BINARY_INTEGER;
	raw_buffer RAW(32767);
	blob_buffer BLOB;
	pos NUMBER;
	rindex BINARY_INTEGER;
	slno BINARY_INTEGER;
	target NUMBER;
	exists_ BOOLEAN;
	file_length NUMBER;
	blocksize NUMBER;
BEGIN
	utl_file.fgetattr(:directory, :original_filename, exists_, file_length, blocksize);
	rindex := dbms_application_info.set_session_longops_nohint;
	select object_id
	  into target
	  from all_objects
	 where object_type = 'DIRECTORY'
	       and object_name = :directory;
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');
	pos := 0;
	LOOP
		BEGIN
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);
			DBMS_LOB.WRITEAPPEND(blob_buffer, UTL_RAW.LENGTH(raw_buffer), raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, 'READ LOB FROM FILE', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_FILE.FCLOSE(f_handle);

	rindex := dbms_application_info.set_session_longops_nohint;
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(blob_buffer);
	f_handle := UTL_FILE.FOPEN(:directory, :uncompressed_filename, 'wb');
	pos := 0;
	LOOP
		BEGIN
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, 'GUNZIP', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_FILE.FCLOSE(f_handle);
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);
	DBMS_LOB.FREETEMPORARY(blob_buffer);
	IF :keep = 0 THEN
		UTL_FILE.FREMOVE(:directory, :original_filename);
	END IF;
END;
