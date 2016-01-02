CREATE OR REPLACE
	AND COMPILE
	JAVA SOURCE NAMED "j_ocp_DirList"
AS
import java.io.File;
import java.io.FilenameFilter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Timestamp;
import oracle.sql.ARRAY;
import oracle.sql.ArrayDescriptor;
import oracle.jdbc.driver.OracleDriver;
import java.util.regex.PatternSyntaxException;

public class j_ocp_DirList
{
	public static ARRAY getList(String directory, String pattern, String hashAlgorithm)
		throws SQLException, NoSuchAlgorithmException
	{
		Connection conn = new OracleDriver().defaultConnection();
		ArrayDescriptor arrayDescriptor = new ArrayDescriptor("T_OCP_FILE_LIST", conn);
		File path = new File(directory);
		File[] files;
		if (pattern != null) {
			final String[] patterns = pattern.split("\0");
			FilenameFilter globFilter = new FilenameFilter() {
				public boolean accept(File dir, String name) {
					for (String pattern : patterns) {
						if (name.matches(Globs.toUnixRegexPattern(pattern))) {
							return true;
						}
					}
					return false;
				}
			};
			files = path.listFiles(globFilter);
		} else {
			files = path.listFiles();
		}
		Object[][] result = new Object[files.length][4];
		for (int i = 0; i < files.length; i++)
		{
			result[i][0] = files[i].getName();
			try
			{
				result[i][1] = new Long(files[i].length());
				result[i][2] = new Timestamp(files[i].lastModified());
				if (hashAlgorithm != null) {
					try {
						FileInputStream inputStream = new FileInputStream(files[i]);
						MessageDigest digest = MessageDigest.getInstance(hashAlgorithm);
						byte[] bytesBuffer = new byte[65536];
						int bytesRead = -1;

						while ((bytesRead = inputStream.read(bytesBuffer)) != -1) {
							digest.update(bytesBuffer, 0, bytesRead);
						}

						result[i][3] = digest.digest();
					}
					catch (FileNotFoundException e) {}
					catch (IOException e) {}
				}
			}
			catch ( java.security.AccessControlException e ) {}
		}
		return new ARRAY(arrayDescriptor, conn, result);
	}
}
