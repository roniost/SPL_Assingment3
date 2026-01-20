package bgu.spl.net.impl.data;

public class SqlTester {
    public static void main(String[] args) {
        System.out.println("=== STARTING SQL INTEGRATION TEST ===");
        
        Database db = Database.getInstance();
        
        // ---------------------------------------------------------
        // מבחן 1: הרשמה וכניסה (משתמש חדש)
        // ---------------------------------------------------------
        System.out.println("\n[Test 1] Registering 'alice'...");
        LoginStatus status1 = db.login(1, "alice", "1234");
        
        if (status1 == LoginStatus.ADDED_NEW_USER) {
            System.out.println("✅ SUCCESS: Alice registered and logged in.");
        } else {
            System.out.println("❌ FAIL: Expected ADDED_NEW_USER, got: " + status1);
        }

        // ---------------------------------------------------------
        // מבחן 2: כניסה של משתמש נוסף
        // ---------------------------------------------------------
        System.out.println("\n[Test 2] Registering 'bob'...");
        LoginStatus status2 = db.login(2, "bob", "pass");
        if (status2 == LoginStatus.ADDED_NEW_USER) {
            System.out.println("✅ SUCCESS: Bob registered and logged in.");
        } else {
            System.out.println("❌ FAIL: Expected ADDED_NEW_USER, got: " + status2);
        }

        // ---------------------------------------------------------
        // מבחן 3: דיווח על קובץ (File Upload)
        // ---------------------------------------------------------
        System.out.println("\n[Test 3] Alice reports a file upload...");
        // הפרמטר השלישי "sports" אמור להתעלם כי אין עמודה כזו ב-SQL
        db.trackFileUpload("alice", "events_monday.json", "sports");
        System.out.println("✅ Action sent (Verify in report below).");

        // ---------------------------------------------------------
        // מבחן 4: יציאה (Logout)
        // ---------------------------------------------------------
        System.out.println("\n[Test 4] Alice logs out...");
        db.logout(1);
        System.out.println("✅ Logout executed.");

        // נסה לעשות לוגין שוב לאליס (כדי לראות היסטוריית כניסות מרובה)
        System.out.println("\n[Test 5] Alice logs in again...");
        db.login(3, "alice", "1234"); // Connection ID חדש
        System.out.println("✅ Alice logged in again.");


        // ---------------------------------------------------------
        // מבחן 5: הדפסת הדוח המלא
        // ---------------------------------------------------------
        String separatorShort = makeSeparator("=", 20);
        String separatorLong = makeSeparator("=", 60);

        System.out.println("\n\n" + separatorShort + " FULL REPORT TEST " + separatorShort);
        System.out.println("Check if the data below matches what we just did:");
        System.out.println("1. Users: alice, bob");
        System.out.println("2. Logins: alice (2 times), bob (1 time)");
        System.out.println("3. Files: alice uploaded 'events_monday.json'");
        System.out.println(separatorLong + "\n");
        
        db.printReport();
        
        System.out.println("\n=== TEST FINISHED ===");
    }

    // פונקציית עזר תואמת Java 8 (במקום String.repeat)
    private static String makeSeparator(String s, int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) {
            sb.append(s);
        }
        return sb.toString();
    }
}