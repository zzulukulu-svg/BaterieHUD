# Battery HUD

Das Programm Battery HUD ist ein kleiner, nützlicher Helfer für deinen Windows-Desktop, der dir auf eine moderne und schicke Art zeigt, wenn dein Laptop geladen oder das Ladekabel getrennt wird. Anstatt nur auf das kleine, oft schwer erkennbare Batterie-Symbol in der Taskleiste zu achten, bietet dir diese App eine klare optische Rückmeldung direkt in der Mitte deines Bildschirms.

<img width="1918" height="1020" alt="image" src="https://github.com/user-attachments/assets/98a780dd-8d98-4596-9908-715ba4214ce0" />

## Was macht Battery HUD?

Sobald du dein Ladekabel in den Laptop steckst, erkennt das Programm dies sofort im Hintergrund. In diesem Moment „ploppt" ein eleganter, leicht durchsichtiger Ring in der Mitte des Desktops auf. Dieser Ring zeigt dir mit einer flüssigen Animation und einer großen Prozentanzeige genau an, wie voll deine Batterie gerade ist.

**Intelligente Farbwahl:**
- Wenn dein Akku fast leer ist (unter 20%), leuchtet die Anzeige in einem warnenden Rot
- Beim Laden erscheint standardmäßig ein beruhigendes Grün
- Beim Ausstecken (optional aktivierbar) zeigt sich ein warmes Orange
- **Neu:** Du kannst beide Farben individuell nach deinem Geschmack anpassen!

<img width="1919" height="1072" alt="Screenshot 2026-01-22 175736" src="https://github.com/user-attachments/assets/df3939e3-c134-48b4-bda4-e89c98200aff" />

Das Besondere daran ist das Design: Die Anzeige wirkt sehr hochwertig, da sie sanft eingeblendet wird, kurz verweilt und dann wie von Geisterhand wieder verschwindet, damit du ungestört weiterarbeiten kannst. Das Programm ist so programmiert, dass es kaum Rechenleistung verbraucht und dein System nicht verlangsamt.

## Neue Features in Version 2.0

### Anpassbare Farben für jede Situation
Du hast jetzt die volle Kontrolle über das Aussehen:
- **Farbe beim Laden** - Wähle deine Lieblingsfarbe für den Ladevorgang
- **Farbe beim Entladen** - Eigene Farbe wenn du das Kabel ziehst
- Beide Farben getrennt einstellbar über den Farbwähler

### Animation beim Ausstecken (Optional)
Neu kannst du wählen, ob die Animation auch erscheinen soll, wenn du das Ladekabel **entfernst**. Diese Funktion ist standardmäßig ausgeschaltet und kann über das Menü aktiviert werden - perfekt, um immer zu wissen, wann dein Laptop auf Akkubetrieb umschaltet!

### Sound-Benachrichtigungen (Optional)
Für noch mehr Feedback kannst du jetzt auch Sounds aktivieren:
- Dezente System-Sounds beim Einstecken und Ausstecken
- Unterschiedliche Töne für Laden und Entladen
- Komplett optional - standardmäßig lautlos

<img width="305" height="218" alt="image" src="https://github.com/user-attachments/assets/e25a1361-f53b-4b51-91ed-5061b6e48d40" />

## Bedienung

Zusätzlich versteckt sich das Tool unauffällig in der Taskleiste neben der Uhr. Dort findest du ein kleines Symbol, über das du mit der rechten Maustaste auf folgende Funktionen zugreifen kannst:

<img width="448" height="330" alt="image" src="https://github.com/user-attachments/assets/eaf86074-3bd3-44f2-86c2-e2e7e029d851" />

**Menü-Optionen:**
- **Animation testen** - Zeigt die Animation sofort an
- **Farbe Laden** - Wähle die Farbe für den Ladevorgang
- **Farbe Entladen** - Wähle die Farbe fürs Ausstecken
- **Farben zurücksetzen** - Zurück zu den Standardfarben
- **Animation beim Ausstecken** - Ein/Aus-Schalter
- **Sound abspielen** - Soundeffekte aktivieren/deaktivieren
- **Beenden** - Programm schließen

## Installation

1. Lade die neueste Version von [Releases](https://github.com/zzulukulu-svg/Aufladen/releases) herunter
2. Starte `BatteryHUD.exe` - keine Installation nötig!
3. Das Programm läuft nun im Hintergrund und ist einsatzbereit

**Optional:** Kopiere die EXE in deinen Autostart-Ordner, damit Battery HUD automatisch mit Windows startet:
```
%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
```

## Einstellungen

Alle deine Anpassungen werden automatisch gespeichert in:
```
%LOCALAPPDATA%\BatteryHUD\config.dat
```

Das bedeutet: Deine gewählten Farben und Einstellungen bleiben erhalten, auch wenn du den PC neu startest!

## Systemanforderungen

- **Betriebssystem:** Windows 7 oder neuer
- **Hardware:** Laptop oder Tablet mit Batterie
- **Speicherplatz:** ca. 200 KB
- **RAM-Verbrauch:** ca. 5-10 MB
- **Installation:** Nicht erforderlich

## Technische Details

Battery HUD ist in nativem C++ geschrieben und nutzt die Windows GDI+ Bibliothek für die schönen Animationen. Das Programm:
- Reagiert sofort auf Batterie-Status-Änderungen
- Nutzt moderne Easing-Funktionen für flüssige Animationen
- Verbraucht minimale Ressourcen durch intelligente Timer-Steuerung
- Läuft komplett im Hintergrund ohne nervige Fenster

## Changelog

### Version 2.0 (Aktuell)
- Separate Farben für Laden & Entladen
- Animation beim Ausstecken (optional)
- Sound-Benachrichtigungen (optional)
- Memory-Leaks behoben
- Performance-Verbesserungen
- Timer-Race-Conditions behoben

### Version 1.0
- Initiales Release
- Animation beim Einstecken
- Einzelne Farbauswahl
- Farbspeicherung

## Tipps & Tricks

**Farben testen:**
Nutze "Animation testen" nachdem du eine neue Farbe gewählt hast, um sofort zu sehen, wie sie aussieht!

**Dezenter Modus:**
Lass "Animation beim Ausstecken" und "Sound abspielen" deaktiviert für minimale Ablenkung.

**Volle Kontrolle:**
Aktiviere alle Features für maximales Feedback über deinen Batterie-Status.

## Beitragen

Du hast Ideen für neue Features oder hast einen Bug gefunden? Erstelle gerne ein [Issue](https://github.com/zzulukulu-svg/Aufladen/issues) oder einen Pull Request!

## Lizenz

Dieses Projekt ist Freeware - nutze es wie du möchtest!

---

**Download:** [Neueste Version herunterladen](https://github.com/zzulukulu-svg/Aufladen/releases/latest)

Es ist also eine einfache, aber sehr elegante Lösung, um den Ladestatus deines Geräts immer im Blick zu haben, ohne danach suchen zu müssen - jetzt mit noch mehr Anpassungsmöglichkeiten!
