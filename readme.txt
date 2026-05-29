Here it is! SRB2 v2.0 source code!
(for the dsi)

----------------------------------------
dependencies:
- blocksds toolchain
- maxmod

----------------------------------------
build instructions:
- run the dsibuild.sh script
- if you didnt mess something up, builds will pop out in "build/src/nds/"

----------------------------------------
music conversion:
this port requires all music to be using a specific format, all vanilla songs have been converted in assets/installer/music.dta

Linux/macOS:
```
for f in *.ogg; do
    ffmpeg -i "$f" -ar 8000 -f s8 -ac 1 "${f%.ogg}.pcm"
done
```

Windows (PS):
```
Get-ChildItem -Filter *.ogg | ForEach-Object { ffmpeg -i $_.FullName -ar 8000 -f s8 -ac 1 "$($_.BaseName).pcm" }
```

----------------------------------------

NOTE: this port is unfinished, do not expect to play cez2 at 35 fps anytime soon
