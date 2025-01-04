# Parsec

A text editor written in C. Currently supporting macOS.

![Preview](assets/preview.png)

## Building

Parsec relies on [Font Stash](https://github.com/memononen/fontstash) for caching glyphs in a texture atlas and 
(Clay)[https://github.com/nicbarker/clay] for laying out UI elements.

The `Makefile` is already configured to download and extract the necessary files from Font Stah and Clay.
