# lpmt
Rewrite of the Little Projection Mapping Tool (http://projection-mapping.org/tools/lpmt/)

LPMT is a quad-based projection mapping tool built on openFrameworks. Each surface can show a
solid colour, an image, a video, a slideshow, a camera, a shared video or an NDI stream, and be
warped (corners, bezier, grid), masked, edge-blended, colour-corrected and animated on a timeline
or driven over OSC and MIDI.

This is a fork of [pierrep/lpmt](https://github.com/pierrep/lpmt), itself a cleanup/rewrite of the
original [hvfrancesco/lpmt](https://github.com/hvfrancesco/lpmt). It builds against
**openFrameworks 0.12.1** for Linux, macOS and Windows.

## What this fork adds
* NDI input as a surface source (sender discovery, frame-synced receive)
* Hardware-accelerated video decode on Linux via GStreamer/VA-API (`make WITH_HWDECODE=1`, see below)
* Reworked OSC interface: stateless `/surface/<n>/<param>` addressing, documented in
  [OSC_ADDRESSES.md](OSC_ADDRESSES.md)
* Source region: show only a part of the content on a surface (`Region X/Y/W/H`, also over OSC)
* Keyboard editing of surfaces: select and nudge corners, scale, rotate, raise/lower layers
* GUI reorganised into `CONTENT` / `LOOK` / `GEOMETRY` pages following the render pipeline

## Screenshots
![Screenshot of LPMT](screenshots/screenshot1.jpg)
![Screenshot of LPMT](screenshots/screenshot2.jpg)
![Screenshot of LPMT](screenshots/screenshot3.jpg)

## OSC control

LPMT listens on port 12345 by default (`bin/data/config.xml` → `OSC:LISTENING_PORT`).
The full address reference is in [OSC_ADDRESSES.md](OSC_ADDRESSES.md);
example controllers live in `bin/data/osc/` (Pure Data patch, TouchOSC layout, JS module) is still for old OSC api, so needs to be modified.

## Installation

Install openFrameworks 0.12.1 following the setup instructions [here](https://openframeworks.cc/download/).

Clone the two external addons into the openFrameworks `addons` folder:

```
cd openFrameworks/addons
git clone https://github.com/danomatika/ofxMidi
git clone https://github.com/leadedge/ofxNDI
```

All other addons are either core ones (`ofxKinect`, `ofxNetwork`, `ofxOpenCv`, `ofxOsc`,
`ofxPoco`, `ofxXmlSettings`) or LPMT-specific versions embedded in `src/` (`ofxTimeline`,
`ofxSimpleGuiToo`, `ofxTween`, `ofxTimecode`, ...).

Then clone this repository into `apps/myApps` (or any folder at the same depth):

```
cd openFrameworks/apps/myApps
git clone https://github.com/pavels/lpmt
```

### NDI runtime

`ofxNDI` loads the NDI library at runtime with `dlopen`/`LoadLibrary`, so nothing is needed to
compile. To actually receive NDI, install the NDI SDK/runtime for your platform:

* Linux: `libndi.so` next to the binary or in `/usr/local/lib` (symlink `libndi.so` → `libndi.so.6`
  if only the versioned file is installed). Source discovery needs a running `avahi-daemon`.
* macOS: `libndi.dylib` in `/usr/local/lib`.
* Windows: the NDI runtime installer (found through `NDI_RUNTIME_DIR_V6`).

Without the runtime LPMT runs normally and NDI surfaces stay blank.

## Building

### Linux
`make` in the project folder builds with the standard `ofVideoPlayer` (GStreamer software decode).

`make WITH_HWDECODE=1` builds the hardware-decode video player (Intel VA-API through the GStreamer
`va` plugin). Requires `pkg-config`, `libgstreamer1.0-dev`, `libgstreamer-plugins-base1.0-dev`,
`libgstreamer-plugins-bad1.0-dev` at build time and the `vah264dec` element at runtime.
Currently limited to MP4/H.264 files; audio is not played in this mode.

A Qt Creator project (`lpmt.qbs`) is also provided. `.vscode/` contains IntelliSense settings
for editing in VS Code.

Kinect support is compiled out by default (`WITH_KINECT` in `ofApp.h` and `quad.h`).

### Windows
Visual Studio 2017 solution (`lpmt.sln`, toolset v141).

### macOS
Xcode project (`lpmt.xcodeproj`). Not tested recently; the project file is kept in sync with the
source tree but may need SDK/signing adjustments in current Xcode versions.
