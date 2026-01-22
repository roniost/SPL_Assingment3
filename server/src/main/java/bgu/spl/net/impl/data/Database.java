	package bgu.spl.net.impl.data;

	import java.io.BufferedReader;
	import java.io.InputStreamReader;
	import java.io.PrintWriter;
	import java.net.Socket;
	import java.util.concurrent.ConcurrentHashMap;

	public class Database {
		private final ConcurrentHashMap<String, User> userMap;
		private final ConcurrentHashMap<Integer, User> connectionsIdMap;
		private final String sqlHost;
		private final int sqlPort;

		private Database() {
			userMap = new ConcurrentHashMap<>();
			connectionsIdMap = new ConcurrentHashMap<>();
			// SQL server connection details
			this.sqlHost = "127.0.0.1";
			this.sqlPort = 7778;

			initialize();
		}

		public static Database getInstance() {
			return Instance.instance;
		}

	    private void initialize() {
	        String result = executeSQL("SELECT username, password FROM users");
		
	        if (result.startsWith("ERROR") || result.equals("EMPTY") || result.trim().isEmpty()) {
	            return;
	        }

	        String[] rows = result.split("\n");
	        for (String row : rows) {
	            String[] parts = row.split("\\|"); 
	            if (parts.length >= 2) {
	                String username = parts[0].trim();
	                String password = parts[1].trim();
	                // Create user in memory but NOT connected (-1)
	                User user = new User(-1, username, password);
	                userMap.put(username, user);
	            }
	        }
	    }

		/**
		 * Execute SQL query and return result
		 * @param sql SQL query string
		 * @return Result string from SQL server
		 */
		private String executeSQL(String sql) {
			try (Socket socket = new Socket(sqlHost, sqlPort);
				PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
				BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()))) {
				
				// Send SQL with null terminator
				out.print(sql + '\0');
				out.flush();
				
				// Read response until null terminator
				StringBuilder response = new StringBuilder();
				int ch;
				while ((ch = in.read()) != -1 && ch != '\0') {
					response.append((char) ch);
				}
				
				return response.toString();
				
			} catch (Exception e) {
				System.err.println("SQL Error: " + e.getMessage());
				return "ERROR:" + e.getMessage();
			}
		}

		/**
		 * Escape SQL special characters to prevent SQL injection
		 */
		private String escapeSql(String str) {
			if (str == null) return "";
			return str.replace("'", "''");
		}

		public void addUser(User user) {
			userMap.putIfAbsent(user.name, user);
		}

		public LoginStatus login(int connectionId, String username, String password) {
			if (connectionsIdMap.containsKey(connectionId)) {
				return LoginStatus.CLIENT_ALREADY_CONNECTED;
			}
			if (addNewUserCase(connectionId, username, password)) {
				// Log new user registration in SQL
				String sql = String.format(
					"INSERT INTO users (username, password) VALUES ('%s', '%s')",
					escapeSql(username), escapeSql(password)
				);
				executeSQL(sql);
				
				// Log login
				logLogin(username);
				return LoginStatus.ADDED_NEW_USER;
			} else {
				LoginStatus status = userExistsCase(connectionId, username, password);
				if (status == LoginStatus.LOGGED_IN_SUCCESSFULLY) {
					// Log successful login in SQL
					logLogin(username);
				}
				return status;
			}
		}

		private void logLogin(String username) {
			String sql = String.format(
				"INSERT INTO logins (username, login_time) VALUES ('%s', datetime('now'))",
				escapeSql(username)
			);
			executeSQL(sql);
		}

		private LoginStatus userExistsCase(int connectionId, String username, String password) {
			User user = userMap.get(username);
			synchronized (user) {
				if (user.isLoggedIn()) {
					return LoginStatus.ALREADY_LOGGED_IN;
				} else if (!user.password.equals(password)) {
					return LoginStatus.WRONG_PASSWORD;
				} else {
					user.login();
					user.setConnectionId(connectionId);
					connectionsIdMap.put(connectionId, user);
					return LoginStatus.LOGGED_IN_SUCCESSFULLY;
				}
			}
		}

		private boolean addNewUserCase(int connectionId, String username, String password) {
			if (!userMap.containsKey(username)) {
				synchronized (userMap) {
					if (!userMap.containsKey(username)) {
						User user = new User(connectionId, username, password);
						user.login();
						addUser(user);
						connectionsIdMap.put(connectionId, user);
						return true;
					}
				}
			}
			return false;
		}

		public void logout(int connectionsId) {
			User user = connectionsIdMap.get(connectionsId);
			if (user != null) {
				// Log logout in SQL
				String sql = String.format(
					"UPDATE logins SET logout_time=datetime('now') " +
					"WHERE username='%s' AND logout_time IS NULL " +
					"ORDER BY login_time DESC LIMIT 1",
					escapeSql(user.name)
				);
				executeSQL(sql);
				
				user.logout();
				connectionsIdMap.remove(connectionsId);
			}
		}

		/**
		 * Track file upload in SQL database
		 * @param username User who uploaded the file
		 * @param filename Name of the file
		 * @param gameChannel Game channel the file was reported to
		 */
		public void trackFileUpload(String username, String filename) { 
			String sql = String.format(
				"INSERT INTO files (username, filename, upload_time) " +
				"VALUES ('%s', '%s', datetime('now'))",
				escapeSql(username), escapeSql(filename)
			);
			executeSQL(sql);
		}

		// Same but with game channel that we ignore
		public void trackFileUpload(String username, String filename,String gameChannel) {
			String sql = String.format(
				"INSERT INTO files (username, filename, upload_time) " +
				"VALUES ('%s', '%s', datetime('now'))",
				escapeSql(username), escapeSql(filename) //escapeSql(gameChannel)
			);
			executeSQL(sql);
		}

		/**
		 * Generate and print server report using SQL queries
		 */
		public void printReport() {
			System.out.println(repeat("=", 80));
			System.out.println("SERVER REPORT - Generated at: " + java.time.LocalDateTime.now());
			System.out.println(repeat("=", 80));
			
			// List all users
			System.out.println("\n1. REGISTERED USERS:");
			System.out.println(repeat("-", 80));
			String usersSQL = "SELECT username FROM users ORDER BY username";
			String usersResult = executeSQL(usersSQL);
			if (usersResult.startsWith("ERROR") || usersResult.equals("EMPTY") || usersResult.trim().isEmpty()) {
				System.out.println("   No users registered (or DB Error)");
			} else {
				String[] rows = usersResult.split("\n");
				if (rows.length > 1) {
					for (String row : rows) {
						if (!row.trim().isEmpty()) {
							System.out.println("   User: " + row.trim());
						}
					}
				}
			}
			
			// Login history for each user
			System.out.println("\n2. LOGIN HISTORY:");
			System.out.println(repeat("-", 80));
			String loginSQL = "SELECT username, login_time, logout_time FROM logins ORDER BY username, login_time DESC";
			String loginResult = executeSQL(loginSQL);

			if (loginResult.startsWith("ERROR") || loginResult.equals("EMPTY") || loginResult.trim().isEmpty()) {
				System.out.println("   No login history");
			} else {
				String[] rows = loginResult.split("\n");
				String currentUser = "";
				for (String row : rows) {
					String[] cols = row.split("\\|");
					if (cols.length >= 3) {
						if (!cols[0].equals(currentUser)) {
							currentUser = cols[0];
							System.out.println("\n   User: " + currentUser);
						}
						System.out.println("      Login:  " + cols[1]);
						
						String logout = cols[2];
						if (logout.equals("None") || logout.equals("null") || logout.isEmpty()) {
							logout = "Still logged in";
						}
						System.out.println("      Logout: " + logout);
					}
				}
			}
			
			// File uploads for each user
			System.out.println("\n3. FILE UPLOADS:");
			System.out.println(repeat("-", 80));
			String filesSQL = "SELECT username, filename, upload_time FROM files ORDER BY username, upload_time DESC";
			String filesResult = executeSQL(filesSQL);
			if (filesResult.startsWith("ERROR") || filesResult.equals("EMPTY") || filesResult.trim().isEmpty()) {
                System.out.println("   No files uploaded");
            } else {
				String[] parts = filesResult.split("\n");
				String currentUser = "";
				for (String part : parts) {
					String[] fields = part.split("\\|");
					if (fields.length >= 3) {
						if (!fields[0].equals(currentUser)) {
							currentUser = fields[0];
							System.out.println("\n   User: " + currentUser);
						}
						System.out.println("      File: " + fields[1]);
						System.out.println("      Time: " + fields[2]);
						System.out.println();
					}
				}

			}
		System.out.println(repeat("=", 80));
	}

	private String repeat(String str, int times) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < times; i++) {
			sb.append(str);
		}
		return sb.toString();
	}

	private static class Instance {
		static Database instance = new Database();
	}}