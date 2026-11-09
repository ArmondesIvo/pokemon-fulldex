# Libraries built into the Android app

- `shizuku-*-13.1.5.jar`: the Shizuku API (https://github.com/RikkaApps/Shizuku-API,
  Apache License 2.0), the `classes.jar` of each of `dev.rikka.shizuku:{api,provider,shared,aidl}:13.1.5`
  from Maven Central. IMPORT SAVES uses it to read another emulator's own folder
  (Android/data/<its package>) when the player has Shizuku running; Android 11 and later let no
  app read another's, but Shizuku runs commands as the shell, which may.
