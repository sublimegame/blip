# Generate icons data for Windows app 

(window + dock icons)

## How to use

From the Git repo root directory:

```bash
./windows/generate-icons/run.sh
```

## What it does

It uses the following files as input:

- `windows/generate-icons/16.rgba`
- `windows/generate-icons/32.rgba`
- `windows/generate-icons/48.rgba`

to generate those output files:

- `xptools/windows/icon_16.hpp`
- `xptools/windows/icon_32.hpp`
- `xptools/windows/icon_48.hpp`

## Notes

The .rgba files have been obtained from a PNG file of the icon, converted on a website (https://convertio.co)