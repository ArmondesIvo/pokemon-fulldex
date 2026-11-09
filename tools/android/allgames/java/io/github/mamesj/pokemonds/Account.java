// PokeHub's account (the figure on the launcher's top bar): signing in with Google through
// PokeHub's own server (tools/pokehub/server, a Cloudflare Worker at BuildInfo.ACCOUNT_URL).
//  - Sign In opens the browser at the server's /auth/start, with a random state kept here.
//  - Google asks the player; the server checks the answer with Google and makes a session, then
//    sends the browser back to pokehub://signin?token=...&state=...&email=...&name=...
//  - GameActivity hands that link here: the state must be the one kept, then the session (its
//    token, email and name) is kept in the app's private preferences, which no other app and no
//    USB cable reads. Sign Out forgets it here (and tells the server, which ends the session).
package io.github.mamesj.pokemonds;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;

import java.net.HttpURLConnection;
import java.net.URL;
import java.security.SecureRandom;

final class Account {
    private static final String TAG = "PokemonDS";

    private Account() {}

    static boolean ready() { return BuildInfo.ACCOUNT_URL != null && !BuildInfo.ACCOUNT_URL.isEmpty(); }

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("account", Context.MODE_PRIVATE);
    }

    // "email\tname" when signed in, else "".
    static String info(Context c) {
        SharedPreferences p = prefs(c);
        String token = p.getString("token", "");
        if (token.isEmpty()) return "";
        return p.getString("email", "") + "\t" + p.getString("name", "");
    }

    static String token(Context c) { return prefs(c).getString("token", ""); }

    static void signIn(final Activity a) {
        if (!ready()) return;
        byte[] b = new byte[24];
        new SecureRandom().nextBytes(b);
        StringBuilder state = new StringBuilder();
        for (byte x : b) state.append(String.format("%02x", x & 0xff));
        prefs(a).edit().putString("state", state.toString()).apply();
        Uri start = Uri.parse(BuildInfo.ACCOUNT_URL + "/auth/start").buildUpon().appendQueryParameter("state", state.toString()).build();
        a.runOnUiThread(() -> {
            try { a.startActivity(new Intent(Intent.ACTION_VIEW, start)); }
            catch (Exception e) { Log.e(TAG, "cannot open the browser to sign in", e); }
        });
    }

    // pokehub://signin?... from the browser. True: it was a sign-in link (kept or not).
    static boolean handle(Context c, Intent intent) {
        if (intent == null || intent.getData() == null) return false;
        Uri u = intent.getData();
        if (!"pokehub".equals(u.getScheme()) || !"signin".equals(u.getHost())) return false;
        SharedPreferences p = prefs(c);
        String want = p.getString("state", "");
        String state = u.getQueryParameter("state"), token = u.getQueryParameter("token");
        // (Only the sign-in this app started: another page cannot sign it in to someone else.)
        if (want.isEmpty() || !want.equals(state) || token == null || token.length() < 32) {
            Log.w(TAG, "sign-in link ignored: not the one asked for");
            return true;
        }
        String email = u.getQueryParameter("email"), name = u.getQueryParameter("name");
        p.edit().remove("state").putString("token", token)
            .putString("email", email == null ? "" : email).putString("name", name == null ? "" : name).apply();
        Log.i(TAG, "signed in as " + (email == null ? "?" : email));
        return true;
    }

    static void signOut(final Context c) {
        final String token = token(c);
        prefs(c).edit().clear().apply();
        if (token.isEmpty() || !ready()) return;
        // (The server ends the session too; if it cannot be reached, the session just expires there.)
        new Thread(() -> {
            try {
                HttpURLConnection h = (HttpURLConnection) new URL(BuildInfo.ACCOUNT_URL + "/auth/signout").openConnection();
                h.setRequestMethod("POST");
                h.setRequestProperty("Authorization", "Bearer " + token);
                h.setConnectTimeout(10000);
                h.setReadTimeout(10000);
                h.getResponseCode();
                h.disconnect();
            } catch (Exception e) { Log.w(TAG, "sign out: server not reached", e); }
        }, "PokeHub-signout").start();
    }
}
