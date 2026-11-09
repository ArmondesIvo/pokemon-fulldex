// IMPORT SAVES (the Hub's page, frontend/launcher.h): what it needs from Android.
//  - A folder chosen with Android's folder picker, where the player put another emulator's saves.
//  - Another emulator's own folder (My Boy's Android/data/com.fastemulator.gba/files/save), read
//    with Shizuku when the player has it running: Android 11 and later let no app read another
//    app's Android/data, not even with "All files access", but Shizuku runs commands as the
//    shell (as adb does), which may. The saves are copied out into a folder of ours; the program
//    then reads them there with ordinary file calls.
package io.github.mamesj.pokemonds;

import android.content.pm.PackageManager;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;

import rikka.shizuku.Shizuku;

final class SaveImport {
    private static final String TAG = "PokemonDS";
    static final int ASK_SHIZUKU = 4211;

    private SaveImport() {}

    // -1 a Shizuku too old for this, 0 none (not installed, or not started since the phone was
    // switched on), 1 running but this app not allowed yet, 2 allowed.
    static int state() {
        try {
            if (!Shizuku.pingBinder()) return 0;
            if (Shizuku.isPreV11()) return -1;
            return Shizuku.checkSelfPermission() == PackageManager.PERMISSION_GRANTED ? 2 : 1;
        } catch (Throwable t) {
            return 0;
        }
    }

    // Shizuku's own question: may this app use it? (Its answer: state() becomes 2.)
    static void ask() {
        try {
            if (Shizuku.pingBinder() && !Shizuku.isPreV11()) Shizuku.requestPermission(ASK_SHIZUKU);
        } catch (Throwable t) {
            Log.e(TAG, "asking Shizuku", t);
        }
    }

    // Copies the .sav files in `from` into `to` (made if need be), as the shell. What it says:
    // the names copied, one a line, or a line starting "ERROR ".
    static String copy(String from, String to) {
        if (state() != 2) return "ERROR SHIZUKU IS NOT ALLOWED";
        if (from.indexOf('\'') >= 0 || to.indexOf('\'') >= 0) return "ERROR A FOLDER'S NAME HAS A QUOTE IN IT";
        final String script = "mkdir -p '" + to + "' || exit 2; cd '" + from + "' || exit 3; "
                + "for f in *.sav *.SAV *.srm *.SRM *.sa1; do [ -f \"$f\" ] && cp -p \"$f\" '" + to + "/' && echo \"$f\"; done; exit 0";
        try {
            // (newProcess is Shizuku's own, kept from other apps' view since API 13 in favour of
            // a "user service"; for one copy, that is a great deal more than needed.)
            Method m = Shizuku.class.getDeclaredMethod("newProcess", String[].class, String[].class, String.class);
            m.setAccessible(true);
            Process p = (Process) m.invoke(null, new String[] { "sh", "-c", script }, null, null);
            String out = readAll(p.getInputStream());
            String err = readAll(p.getErrorStream());
            int code = p.waitFor();
            if (code == 3) return "ERROR NOTHING THERE: " + from;
            if (code != 0) return "ERROR " + (err.isEmpty() ? "THE COPY FAILED (" + code + ")" : err.trim());
            return out;
        } catch (Throwable t) {
            Log.e(TAG, "copying with Shizuku", t);
            return "ERROR " + t.getClass().getSimpleName() + ": " + t.getMessage();
        }
    }

    private static String readAll(InputStream in) throws java.io.IOException {
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        for (int n; (n = in.read(buf)) > 0; ) b.write(buf, 0, n);
        return b.toString("UTF-8");
    }
}
