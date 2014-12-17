# Vango

Painterly representation of images.

![Our glorious campus](./results/columbia2_textured.png)

## Usage

### Dependencies

This project has a few dependencies, which CMake will attempt to find:

- [**OpenCV**](http://opencv.org/): Open-Source Computer Vision Library
- [**Boost**](http://www.boost.org/): Peer-reviewed portable C++ source libraries
- [**TCLAP**](http://tclap.sourceforge.net/): Templatized C++ Command Line Parser Library
- [**yaml-cpp**](https://code.google.com/p/yaml-cpp/): YAML Parser and Emitter

You can find and install these using apt-get, [Homebrew](http://brew.sh/), or your favorite package manager. Or you can install them from source!

### Compilation

Once you've installed these, you can run CMake on each of the subdirectories `process` and `render`. Our preferred method is to make build directories in each of the directories, like so:

    > cd vango
    > mkdir process/build
    > cd process/build
    > cmake ..
    > make
    > cd -
    > mkdir render/build
    > cd render/build
    > cmake ..
    > make

The included bash script `compile.sh` also does this for you.

### Running our code!

There are two main executables in the process of creating a painted image: `./process` and `./render`. `process` will take an input PNG and output a YAML file representation of the resulting "canvas," like so:

    > cd process/build
    > ./process -i ../../assets/input_images/columbia2.png -s ../../assets/styles/columbia2.yaml -o ../../assets/canvas/columbia2.yaml

Notice that there is one extra parameter noted by `-s`: this denotes the "style" file, which includes parameters to be passed into both the process and render steps. You want to pass in this same style file when you render the canvas, like so:

    > cd -
    > cd render/build
    > ./render ../../assets/canvas/columbia2.yaml -s ../../assets/styles/columbia2.yaml -O

The `-O` flag will output a PNG filename similar to the input canvas filename; in this case, our output will be written to `./columbia2.png`. You can specify a custom output path by using the `-o` flag instead. Run `./render --help` for more information.

The included bash script `./run.sh` will run both the executables, given that input images and styles are both provided in the `assets/input_images` and `assets/styles` directories, with the same filename. So given the above file configuration, in order to paint `columbia2.png` one would type:

    > ./run.sh columbia2

`./run.sh` assumes that input images are in PNG format.

## Setup

This project is split into two main tasks:

1. Image Processing / Brush stroke creation.
    - Load up an image, and run OpenCV operations on it.
    - Output a YAML canvas file.
2. Image Rendering
    - Parse the YAML canvas file.
    - Output a rendered image.

More information about each of these processes are included in their own README's: The Process README is located at `process/README.md` and the Render README is located at `render/README.md`.

## File Format

There are two YAML files necessary for this project. 
The first, provided by the user, represents a parameterization of Style and is
used in the determination of how the image is processed and rendered. 
It contains the following:

- Canvas
    + canvas_scale: The canvas size will be scaled up from the image by this value
    + bg: Textured surface to be painted on
        * opacity: Opacity of background texture (e.g. .2)
        * tex_path: Path to texture file 
    + layers: Each layer will have its own set of parameters
        * regen_width: Spacing of stroke-placement. If larger than brush_width, canvas will show through
        * avg_brush_width: Average width of strokes per layer (largest on lower layers)
        * var_brush_width: Max variation for width of strokes
        * max_brush_length: The upper limit to length of strokes. Will probably be clipped to shorter lengths though
        * opacity: Opacity at which brushstroke is drawn
        * regen_mask_width: Controls the size of the area at which new brushstrokes are generated. If 0, as it should be for the bottom layer, the whole canvas will be covered. Otherwise, it will be restricted to high-frequency regions. 
        * clip_threshold: The gradient difference at which an image is clipped (e.g. 1.0) If larger, less clipping will occur
        * strength_threshold: In progress
        * strength_neighborhood: In progress
        * tex_path: Path to texture image
        * mask_path: Path to alpha mask image
        * tex_blend: Blending factor of the brush texture. When 0, brush texture has no effect
        * tex_spacing: The rate at which alpha mask is clone-stamped (usually 1.0) 
        * tex_jitter: The angle variation during clone_stamping (usually 0-.3)


The second YAML file, which will be automatically generated by the process algorithm
for the given image and style parameters, explicitely defines the canvas, with all
associated layers and strokes, of the rendered image. 



