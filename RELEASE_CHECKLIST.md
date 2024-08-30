# Release Checklist

## Bump Versions

- `yoyoengine/editor/include/editor.h`

## Create Tag

Create and push a tag on the main branch to freeze the ref.

## Create Release

- Locally, build the release with `yoyoeditor/build_linux.sh`
- rename `build/out/bin/linux` to `build/out/bin/yoyoeditor`
- create tar gzip with `tar -czvf yoyoeditor.tar.gz build/out/bin/yoyoeditor`
- create a release on github with the tag and attach the tar.gz file
- add patch notes
