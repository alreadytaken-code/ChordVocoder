# Chord Vocoder (VST3)

Vocoder per voce con carrier integrato (synth polifonico) che segue un accordo.

## Compilare su Windows
1. Installa Visual Studio 2022 (workload "Sviluppo di applicazioni desktop con C++") e CMake.
2. Da "Developer PowerShell for VS 2022", nella cartella del progetto:
       cmake -B build -G "Visual Studio 17 2022"
       cmake --build build --config Release --target ChordVocoder_VST3
   (il primo giro scarica JUCE da GitHub, serve internet e qualche minuto)
3. Copia la cartella `build\ChordVocoder_artefacts\Release\VST3\Chord Vocoder.vst3`
   in `C:\Program Files\Common Files\VST3`.
4. In FL Studio: Options > Manage plugins > Find installed plugins.

## Uso in FL Studio
- Mettilo come effetto su un canale mixer con la voce (modulatore).
- Chord Source = "Internal chord": scegli Root, Type e Octave, la voce segue quell'accordo.
- Chord Source = "MIDI input": suona note/accordi da un piano roll e instrada il MIDI
  verso il plugin (impostazione "Input port" nel wrapper del plugin).
- Preset: menu "Preset" in alto nell'interfaccia.
