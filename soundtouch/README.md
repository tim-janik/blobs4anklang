# SoundTouch

This directory contains instructions for mirroring SoundTouch tarballs.
We provide this mirrored release to ensure a reliable download location for
the Anklang project (e.g., during CI), as the original host (surina.net)
may have rate limits.

## Origin
The official source for SoundTouch is:
https://www.surina.net/soundtouch/

The specific file mirrored here is:
https://www.surina.net/soundtouch/soundtouch-*.tar.gz

## Re-release Procedure

To mirror the file and upload it as a GitHub release:

```bash
V=2.4.0 && FILE="soundtouch-$V.tar.gz"

# Download the tarball
wget -c "https://www.surina.net/soundtouch/${FILE}"

# Create a suitable tag
git tag -d "soundtouch-v${V}"
git tag "soundtouch-v${V}"

# Upload to GitHub Releases
gh release create \
  --title "SoundTouch ${V}" \
  --draft --target=$(git rev-parse HEAD) \
  soundtouch-v$V \
  --notes "Mirrored release of SoundTouch ${V} for reliable download location." \
  "${FILE}"

# Puiblish tag
git push origin "soundtouch-v${V}"
```

## Credits
SoundTouch is licensed LGPL-2.1 and Copyright by Olli Parviainen.
No modifications have been made to the tarball; it is a direct mirror of the original release.
