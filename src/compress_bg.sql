DECLARE
	safe_directory_ varchar2(60);
	safe_compression_level_ varchar2(1);
	safe_original_filename_ varchar2(512);
	safe_compressed_filename_ varchar2(512);
BEGIN
	safe_directory_ := dbms_assert.enquote_literal(''''||replace(:directory, '''', '''''')||'''');
	safe_compression_level_ := to_char(:compression_level, 'TM', 'NLS_Numeric_Characters = ''.,''');
	safe_original_filename_ := dbms_assert.enquote_literal(''''||replace(:original_filename, '''', '''''')||'''');
	safe_compressed_filename_ := dbms_assert.enquote_literal(''''||replace(:compressed_filename, '''', '''''')||'''');

	:job_name := dbms_scheduler.generate_job_name('OCP_GZIP_');
	dbms_scheduler.create_job (
		job_name => :job_name,
		job_type => 'PLSQL_BLOCK',
		enabled => TRUE,
		comments => 'ocp compression job',
		job_action => '
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
	utl_file.fgetattr(' || safe_directory_ || ', ' || safe_original_filename_ || ', exists_, file_length, blocksize);
	rindex := dbms_application_info.set_session_longops_nohint;
	select object_id
	  into target
	  from all_objects
	 where object_type = ''DIRECTORY''
	       and object_name = ' || safe_directory_ || ';
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');
	pos := 0;
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);
	' || case when :compression_level > 0 then
		'c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer, ' || safe_compression_level_ || ');'
	else
		'c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(blob_buffer);'
	end || '
	LOOP
		BEGIN
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);
			UTL_COMPRESS.LZ_COMPRESS_ADD(c_handle, blob_buffer, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, ''GZIP'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_COMPRESS.LZ_COMPRESS_CLOSE(c_handle, blob_buffer);
	UTL_FILE.FCLOSE(f_handle);

	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_compressed_filename_ || ', ''wb'');
	pos := 0;
	blobsize := DBMS_LOB.GETLENGTH(blob_buffer);
	rindex := dbms_application_info.set_session_longops_nohint;
	WHILE pos < blobsize LOOP
		actual_size := LEAST(blobsize - pos, 16384);
		DBMS_LOB.READ(blob_buffer, actual_size, pos + 1, raw_buffer);
		UTL_FILE.PUT_RAW(f_handle, raw_buffer);
		pos := pos + actual_size;
		dbms_application_info.set_session_longops(rindex, slno, ''WRITE LOB TO FILE'', target, 0, pos, blobsize, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');
	END LOOP;
	DBMS_LOB.FREETEMPORARY(blob_buffer);
	UTL_FILE.FCLOSE(f_handle);
	' || case when :keep = 0 then
		'UTL_FILE.FREMOVE(' || safe_directory_ || ', ' || safe_original_filename_ || ');'
	end || '
END;
');
END;
