# Parsec

A text editor written in C. Currently supporting macOS.

![Preview](assets/preview.png)

## Building

A single command to `make` is all that is needed to full compile Parsec.

Parsec relies on [Font Stash](https://github.com/memononen/fontstash) for caching glyphs in a texture atlas and 
[Clay](https://github.com/nicbarker/clay) for laying out UI elements. The `Makefile` is already configured to extract
the necessary files from both repos.
