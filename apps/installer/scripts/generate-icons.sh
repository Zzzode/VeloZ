#!/bin/bash
# Generate application icons from SVG source
# Requires: ImageMagick (convert) or librsvg (rsvg-convert)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ASSETS_DIR="$(cd "$SCRIPT_DIR/../assets" && pwd)"
ICONS_DIR="$ASSETS_DIR/icons"
SVG_SOURCE="$ICONS_DIR/icon.svg"

echo "Generating icons from: $SVG_SOURCE"

# Check for required tools
if command -v rsvg-convert &> /dev/null; then
    CONVERTER="rsvg"
elif command -v convert &> /dev/null; then
    CONVERTER="imagemagick"
else
    echo "Error: Neither rsvg-convert nor ImageMagick found"
    echo "Install with:"
    echo "  macOS: brew install librsvg imagemagick"
    echo "  Ubuntu: sudo apt install librsvg2-bin imagemagick"
    exit 1
fi

# Generate PNG at various sizes
generate_png() {
    local size=$1
    local output=$2

    if [ "$CONVERTER" = "rsvg" ]; then
        rsvg-convert -w "$size" -h "$size" "$SVG_SOURCE" -o "$output"
    else
        convert -background none -resize "${size}x${size}" "$SVG_SOURCE" "$output"
    fi
    echo "  Created: $output (${size}x${size})"
}

# Generate standard PNG sizes
echo "Generating PNG icons..."
generate_png 16 "$ICONS_DIR/icon-16.png"
generate_png 32 "$ICONS_DIR/icon-32.png"
generate_png 48 "$ICONS_DIR/icon-48.png"
generate_png 64 "$ICONS_DIR/icon-64.png"
generate_png 128 "$ICONS_DIR/icon-128.png"
generate_png 256 "$ICONS_DIR/icon-256.png"
generate_png 512 "$ICONS_DIR/icon-512.png"
generate_png 1024 "$ICONS_DIR/icon-1024.png"

# Copy 256px as main icon.png
cp "$ICONS_DIR/icon-256.png" "$ICONS_DIR/icon.png"
echo "  Created: icon.png (256x256)"

# Generate tray icon (smaller, for system tray)
generate_png 22 "$ICONS_DIR/tray-icon.png"
echo "  Created: tray-icon.png (22x22)"

# Generate Windows ICO
if command -v convert &> /dev/null; then
    echo "Generating Windows ICO..."
    convert "$ICONS_DIR/icon-16.png" \
            "$ICONS_DIR/icon-32.png" \
            "$ICONS_DIR/icon-48.png" \
            "$ICONS_DIR/icon-64.png" \
            "$ICONS_DIR/icon-128.png" \
            "$ICONS_DIR/icon-256.png" \
            "$ICONS_DIR/icon.ico"
    echo "  Created: icon.ico"
else
    echo "Skipping ICO generation (requires ImageMagick)"
fi

# Generate macOS ICNS
if [ "$(uname)" = "Darwin" ]; then
    echo "Generating macOS ICNS..."

    ICONSET_DIR="$ICONS_DIR/icon.iconset"
    mkdir -p "$ICONSET_DIR"

    # Copy icons to iconset with required names
    cp "$ICONS_DIR/icon-16.png" "$ICONSET_DIR/icon_16x16.png"
    cp "$ICONS_DIR/icon-32.png" "$ICONSET_DIR/icon_16x16@2x.png"
    cp "$ICONS_DIR/icon-32.png" "$ICONSET_DIR/icon_32x32.png"
    cp "$ICONS_DIR/icon-64.png" "$ICONSET_DIR/icon_32x32@2x.png"
    cp "$ICONS_DIR/icon-128.png" "$ICONSET_DIR/icon_128x128.png"
    cp "$ICONS_DIR/icon-256.png" "$ICONSET_DIR/icon_128x128@2x.png"
    cp "$ICONS_DIR/icon-256.png" "$ICONSET_DIR/icon_256x256.png"
    cp "$ICONS_DIR/icon-512.png" "$ICONSET_DIR/icon_256x256@2x.png"
    cp "$ICONS_DIR/icon-512.png" "$ICONSET_DIR/icon_512x512.png"
    cp "$ICONS_DIR/icon-1024.png" "$ICONSET_DIR/icon_512x512@2x.png"

    # Convert to ICNS
    iconutil -c icns "$ICONSET_DIR" -o "$ICONS_DIR/icon.icns"

    # Clean up iconset
    rm -rf "$ICONSET_DIR"

    echo "  Created: icon.icns"
else
    echo "Skipping ICNS generation (requires macOS)"
fi

echo ""
echo "Icon generation complete!"
echo "Generated files:"
ls -la "$ICONS_DIR"
