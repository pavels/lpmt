# LPMT OSC addresses

Default listening port: **12345** (`config.xml` → `OSC:LISTENING_PORT`).

There is no "currently selected surface" in the OSC interface. Every surface message
carries its own target index, so a sender never has to track receiver state:

    /surface/<n>/<parameter>      n is the surface index, 0 .. 35

Messages for a surface that does not exist are ignored and logged.

## Argument conventions

| Parameter kind | With an argument            | With no argument |
|----------------|-----------------------------|------------------|
| boolean        | non-zero = on, zero = off   | toggles          |
| float / int    | sets the value (clamped)    | ignored          |
| action         | -                           | performs it      |

Ints, floats, strings, and `T`/`F` are accepted interchangeably wherever a number is
expected. Values are clamped to the same ranges as the corresponding GUI sliders.

## Per-surface parameters

    show                            enable/disable the surface

    hue                             0 .. 1
    saturation                      0 .. 1
    luminance                       0 .. 1

    img                             image content on/off  (alias: img/show)
    img/path <string>               load an image by file path
    img/blob <appid> <blob>         load an image from an OSC blob
    img/load                        open the image file dialog
    img/hmirror, img/vmirror        mirror the image
    img/fit, img/keepaspect         fit to surface / preserve aspect ratio
    img/mult/x, img/mult/y          scale, 0.1 .. 5.0
    img/color <r> <g> <b> <a>       colorize, each 0 .. 1
    img/color/1 .. img/color/4      one component at a time (1=r, 4=a)

    video                           video content on/off  (alias: video/show)
    video/path <string>             load a video by file path
    video/load                      open the video file dialog
    video/speed                     -2.0 .. 4.0
    video/volume                    0 .. 1
    video/loop                      loop on/off

    sharedvideo                     shared video on/off   (alias: sharedvideo/show)
    sharedvideo/num                 which shared video, 1 .. 8
    sharedvideo/tiling              tiling on/off

    slideshow                       slideshow on/off      (alias: slideshow/show)
    slideshow/load                  open the slideshow folder dialog
    slideshow/folder                slideshow index
    slideshow/duration              seconds per slide, 0.1 .. 15.0
    slideshow/transitions           fade transitions on/off

    cam                             camera content on/off (alias: cam/show)
    cam/select <int>                which camera, by index

    solid                           solid colour on/off   (alias: solid/show)

    mask                            mask on/off           (alias: mask/show)
    mask/invert, mask/outline

    greenscreen                     on/off                (alias: greenscreen/show)
    greenscreen/threshold           0 .. 255
    greenscreen/color <r> <g> <b> <a>
    greenscreen/color/1 .. /4

    blendmodes                      on/off                (alias: blendmodes/show)
    blendmodes/mode                 0 .. 5

    edgeblend                       on/off                (alias: edgeblend/show)
    edgeblend/power                 0 .. 4
    edgeblend/gamma                 0 .. 4
    edgeblend/luminance             -4 .. 4
    edgeblend/amount <l> <r> <t> <b>
    edgeblend/amount/left|right|top|bottom     each 0 .. 0.5

    placement <x> <y>               content offset
    placement/x, placement/y        -1600 .. 1600
    placement/dimensions <w> <h>
    placement/w, placement/h        0 .. 2400

    corners/<c> <x> <y>             c is 0 .. 3
    corners/<c>/x, corners/<c>/y

    crop/rectangular/top|right|bottom|left     each 0 .. 1
    crop/circular/x, crop/circular/y           0 .. 1
    crop/circular/radius                       0 .. 2

    deform/bezier                   bezier warping on/off
    deform/grid                     grid warping on/off
    deform/grid/rows                2 .. 15
    deform/grid/columns             2 .. 20

    timeline/tint, timeline/color, timeline/alpha, timeline/slides

Kinect parameters (`kinect/...`) are registered only in builds with `WITH_KINECT`.

## Global

    /projection/start                       start projection
    /projection/stop                        stop projection
    /projection/resync                      resync videos and slideshows
    /projection/save [path]                 save (default: _lpmt_settings.xml)
    /projection/load [path]                 load (default: _lpmt_settings.xml)
    /projection/fullscreen [0|1]
    /projection/gui [0|1]
    /projection/mode/setup [0|1]
    /projection/mode/masksetup [0|1]        ignored while the gui is shown
    /projection/timeline/use [0|1]
    /projection/timeline/show [0|1]
    /projection/timeline/play               toggles playback
    /projection/timeline/duration <sec>     minimum 10
    /projection/sharedvideo/path <n> <path> load shared video n (1 .. 8)

## Changed from earlier versions

* `/active/...` and `/active/set` are gone. Use `/surface/<n>/...`; in most cases the
  only change is the address prefix.
* `/projection/fullscreen/on|off|toggle` became `/projection/fullscreen [0|1]`.
  The same applies to `gui`, `mode/setup`, `mode/masksetup` and `timeline/use`.
* `/projection/timeline/start` became `/projection/timeline/play`.
* `/projection/save` no longer opens a dialog; it writes the project file directly.
* `/corners/x|y <quad> <corner> <v>` became `/surface/<n>/corners/<c>/x|y <v>`.
* `/image <quad> <appid> <blob>` became `/surface/<n>/img/blob <appid> <blob>`.
* `/projection/mpe/connect` is gone (the MPE code it called is commented out).
* OSC "learning" is gone. Every parameter now has a fixed address.
