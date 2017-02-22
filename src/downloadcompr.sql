DECLARE
	f_handle UTL_FILE.FILE_TYPE;
	c_handle BINARY_INTEGER;
	raw_buffer RAW(32767);
	pos NUMBER;
	rindex BINARY_INTEGER;
	slno BINARY_INTEGER;
	target NUMBER;
	exists_ BOOLEAN;
	file_length NUMBER;
	blocksize NUMBER;
BEGIN
	utl_file.fgetattr(:directory, :filename, exists_, file_length, blocksize);
	rindex := dbms_application_info.set_session_longops_nohint;
	select object_id
	  into target
	  from all_objects
	 where object_type = 'DIRECTORY'
	       and object_name = :directory;
	f_handle := UTL_FILE.FOPEN(:directory, :filename, 'rb');
	pos := 0;
	if :skipbytes > 0 then
		declare
			leftToSkip_ number := :skipbytes;
			size_ number;
		begin
			while leftToSkip_ > 0 loop
				size_ := least(leftToSkip_, 16384);
				utl_file.get_raw(f_handle, raw_buffer, size_);
				leftToSkip_ := leftToSkip_ - utl_raw.length(raw_buffer);
				pos := pos + utl_raw.length(raw_buffer);
				dbms_application_info.set_session_longops(rindex, slno, 'GZIP', target, 0, pos, file_length, :directory || ':' || :filename, 'bytes');
			end loop;
		end;
	end if;
	DBMS_LOB.CREATETEMPORARY(:blob, TRUE, DBMS_LOB.CALL);
	IF :compression_level > 0 THEN
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(:blob, :compression_level);
	ELSE
		c_handle := UTL_COMPRESS.LZ_COMPRESS_OPEN(:blob);
	END IF;
	LOOP
		BEGIN
			UTL_FILE.GET_RAW(f_handle, raw_buffer, 16384);
			UTL_COMPRESS.LZ_COMPRESS_ADD(c_handle, :blob, raw_buffer);
			pos := pos + utl_raw.length(raw_buffer);
			dbms_application_info.set_session_longops(rindex, slno, 'GZIP', target, 0, pos, file_length, :directory || ':' || :filename, 'bytes');
		EXCEPTION
			WHEN NO_DATA_FOUND THEN
				EXIT;
		END;
	END LOOP;
	UTL_COMPRESS.LZ_COMPRESS_CLOSE(c_handle, :blob);
	UTL_FILE.FCLOSE(f_handle);
END;
