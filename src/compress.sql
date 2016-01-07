DECLARE
	f_handle UTL_FILE.FILE_TYPE;
	c_handle BINARY_INTEGER;
	raw_buffer RAW(32767);
	blob_buffer BLOB;
	actual_size NUMBER;
	pos NUMBER;
	blobsize NUMBER;
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
	f_handle := UTL_FILE.FOPEN(:directory, :original_filename, 'rb');
	pos := 0;
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);
	IF :compression_level > 0 THEN
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer, :compression_level);
	ELSE
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer);
	END IF;
	LOOP
		BEGIN
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);
			UTL_COMPRESS.LZ_COMPRESS_ADD(c_handle, blob_buffer, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, 'GZIP', target, 0, pos, file_length, :directory || ':' || :original_filename, 'bytes');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_COMPRESS.LZ_COMPRESS_CLOSE(c_handle, blob_buffer);
	UTL_FILE.FCLOSE(f_handle);

	f_handle := UTL_FILE.FOPEN(:directory, :compressed_filename, 'wb');
	pos := 0;
	blobsize := DBMS_LOB.GETLENGTH(blob_buffer);
	rindex := dbms_application_info.set_session_longops_nohint;
	WHILE pos < blobsize LOOP
		actual_size := LEAST(blobsize - pos, 16384);
		DBMS_LOB.READ(blob_buffer, actual_size, pos + 1, raw_buffer);
		UTL_FILE.PUT_RAW(f_handle, raw_buffer);
		pos := pos + actual_size;
		dbms_application_info.set_session_longops(rindex, slno, 'WRITE LOB TO FILE', target, 0, pos, blobsize, :directory || ':' || :original_filename, 'bytes');
	END LOOP;
	DBMS_LOB.FREETEMPORARY(blob_buffer);
	UTL_FILE.FCLOSE(f_handle);
	IF :keep = 0 THEN
		UTL_FILE.FREMOVE(:directory, :original_filename);
	END IF;
END;
