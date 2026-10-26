# data/

The original game files go here. They are not committed.

```
python tools/extract_assets.py --apk <ProjectY .apk> --maps <folder with Map*.zip and Cmp1_4*.zip>
```

Layout (mirrors where the original engine looked for files):

- `data/<file>`: everything from the APK's `assets/` folder (config.txt, strings, meshes, textures, sounds, shaders)
- `data/add/<file>`: the map and campaign files from the downloaded map archives

The tool checks files against the game's own `md5.txt`.
