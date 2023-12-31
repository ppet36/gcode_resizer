# GCode Resizer

A simple utility to resize a GCode file and move it to the origin of 0.0. I use it to output from DrawingBot or Inkscape for my plotter. 

This C implementation does not load the file into memory and instead loops through it twice. The first time to find out the dimensions and displacement and the second time for the transformation itself. The resulting gcode is written to standard output. I created it because existing tools, mostly in Python, are very slow for large gcode files which specially DrawingBot produces.

## Compiling

There are no extra dependencies exists. Simply compile with gcc or other compiler eq:

```
gcc gcode_resizer.c -o gcode_resizer
```

## Using

It is very simple to use. The command is passed a gcode file and the required range to "fit" into. It is possible to pass only one dimension, then it will be the longer side of the resulting drawing.

```
./gcode_resizer sample.gcode 210x180 >sample_converted.gcode
```
where sample.gcode will be fitted to 210x180mm or:
```
./gcode_resizer sample.gcode 150 >sample_converted.gcode
```
where largest side of drawing will be 150mm. Optionally you can rotate gcode file with parameter `--rotate` if you need. For example:
```
./gcode_resizer sample.gcode 180x210 --rotate >sample_rotated_converted.gcode
```

## Limitations

The command only handles simple gcode and transforms only X, Y, I and J coordinates.
