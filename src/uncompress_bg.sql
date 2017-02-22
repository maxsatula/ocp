DECLARE
	safe_directory_ varchar2(60);
	safe_original_filename_ varchar2(512);
	safe_uncompressed_filename_ varchar2(512);
BEGIN
	safe_directory_ := dbms_assert.enquote_literal(''''||replace(:directory, '''', '''''')||'''');
	safe_original_filename_ := dbms_assert.enquote_literal(''''||replace(:original_filename, '''', '''''')||'''');
	safe_uncompressed_filename_ := dbms_assert.enquote_literal(''''||replace(:uncompressed_filename, '''', '''''')||'''');

	:job_name := dbms_scheduler.generate_job_name('OCP_GUNZIP_');
	dbms_scheduler.create_job (
		job_name => :job_name,
		job_type => 'PLSQL_BLOCK',
		enabled => TRUE,
		comments => 'ocp decompression job',
		job_action => '
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
	utl_file.fgetattr(' || safe_directory_ || ', ' || safe_original_filename_ || ', exists_, file_length, blocksize);
	rindex := dbms_application_info.set_session_longops_nohint;
	select object_id
	  into target
	  from all_objects
	 where object_type = ''DIRECTORY''
	       and object_name = ' || safe_directory_ || ';
	DBMS_LOB.CREATETEMPORARY(blob_buffer, TRUE, DBMS_LOB.CALL);
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_original_filename_ || ', ''rb'');
	pos := 0;
	LOOP
		BEGIN
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);
			DBMS_LOB.WRITEAPPEND(blob_buffer, UTL_RAW.LENGTH(raw_buffer), raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, ''READ LOB FROM FILE'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_FILE.FCLOSE(f_handle);

	rindex := dbms_application_info.set_session_longops_nohint;
	c_handle := UTL_COMPRESS.LZ_UNCOMPRESS_OPEN(blob_buffer);
	f_handle := UTL_FILE.FOPEN(' || safe_directory_ || ', ' || safe_uncompressed_filename_ || ', ''wb'');
	pos := 0;
	LOOP
		BEGIN
			UTL_COMPRESS.LZ_UNCOMPRESS_EXTRACT(c_handle, raw_buffer);
			UTL_FILE.PUT_RAW(f_handle, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, ''GUNZIP'', target, 0, pos, file_length, ' || safe_directory_ || ' || '':'' || ' || safe_original_filename_ || ', ''bytes'');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_FILE.FCLOSE(f_handle);
	UTL_COMPRESS.LZ_UNCOMPRESS_CLOSE(c_handle);
	DBMS_LOB.FREETEMPORARY(blob_buffer);
	' || case when :keep = 0 then
		'UTL_FILE.FREMOVE(' || safe_directory_ || ', ' || safe_original_filename_ || ');'
	end || '
END;
');
END;
