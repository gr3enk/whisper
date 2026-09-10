# Whisper ASMR

Ein kompaktes JUCE Audio-Plugin für ASMR-Aufnahmen. Vier Makro-Regler formen
Wärme, Sprachverständlichkeit, luftige Details und Dynamik; ein Output-Regler
gleicht den Pegel ab.

## Regler

- **Warmth** – breites Low-Shelf bei 230 Hz
- **Clarity** – Präsenzband um 3,2 kHz
- **Tingles** – Air-Shelf ab 9 kHz
- **Compression** – kombiniert Ratio (3:1–6:1), Attack (10–1 ms), Release
  (300–100 ms) und Makeup-Gain (0–5 dB)
- **Output** – finaler Pegel von -18 bis +24 dB

Mit **Learn Threshold** werden zehn Sekunden des Eingangssignals analysiert. Ein
robustes Perzentil der aktiven Abschnitte setzt den Threshold, ohne dass einzelne
laute Ausreißer oder sehr leise Pausen das Ergebnis dominieren. Ein zweiter Klick
beendet die Messung vorzeitig.

Ein stereo-gekoppelter Lookahead-Peak-Limiter hinter dem Output-Gain begrenzt
kurze, vom Kompressor bewusst durchgelassene Transienten auf -1 dBFS. Die dafür
benötigten 2,5 ms Latenz werden dem Host gemeldet und von der DAW kompensiert.

Vor dem Makeup-Gain fängt ein zweiter, stereo-gekoppelter Peak-Kompressor kurze
Spitzen ab. Er arbeitet mit sofortiger Attack, 12:1 Ratio, 80 ms Release und
einem Threshold 12 dB oberhalb des gelernten Haupt-Thresholds. So passt sich die
Peak-Stufe auch an sehr leise Aufnahmen an.

Zusätzlich entfernt ein Hochpass bei 65 Hz Trittschall und tieffrequenten Rumble.
Alle Werte sind DAW-automatisierbar und werden mit dem Projekt gespeichert.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Erzeugt werden VST3, Audio Unit (macOS) und eine Standalone-App.

## GitHub Release

Unter **Actions → Create Release → Run workflow** kann ein Release manuell
gestartet werden. Die Auswahl `major`, `minor` oder `patch` erhöht die Version in
`CMakeLists.txt` nach SemVer, baut universelle macOS-Versionen für Intel und Apple
Silicon und veröffentlicht AU, VST3, Standalone sowie SHA-256-Prüfsummen in einem
GitHub Release.

Der Workflow benötigt Schreibzugriff auf den gewählten Branch. Bei geschützten
Branches muss GitHub Actions das Pushen erlauben oder der Workflow von einem
geeigneten Release-Branch gestartet werden.
