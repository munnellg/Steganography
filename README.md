# Steganography

Steganography is the process of hiding data with data. In the case of this
application I hide a file inside an image by manipulating the two least 
significant bits of the each colour channel of an image to store a file.
The change in colour is imperceptable, but anyone with enough knowhow should
be able to pull the data back out.

This application works in both directions - embedding and retrieving.

By default the application attempts to retrieve data from an image. To embed
a file, use the application like so:

`./steg -e file.tar.gz image.png`

By default the output image is called `out.png`. You can specify the name of
the output file like so:

`./ste -e file.tar.gz -o encoded.png image.png`

You may retrieve your file from the output image like so:

`./steg encoded.png`

Again, should you wish to specify the name of the output file, you may do so
using the -o flag:

`./steg -o diff_name.tar.gz encoded.png`

My application uses the CImg library for image processing. It also uses boost
(very briefly) to strip filepaths from the embedded file.
