#include "CImg.h"
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

/* some helpful macros for determining things like the number of pixel channels
 * required to encode a unit of information or masks for clearing/setting bits
 * during encoding/retrieval */
#define BYTES_TO_BITS(x)        ((x)*CHAR_BIT)
#define NUM_CHANNEL_BITS        BYTES_TO_BITS(sizeof(CHANNEL))
#define ENCODE_BITS_PER_CHANNEL 2
#define CHANNEL_BIT_MASK        ((((uint64_t) 1) << ENCODE_BITS_PER_CHANNEL)-1)
#define CHANNELS_TO_ENCODE(x)   ( BYTES_TO_BITS((x))/ENCODE_BITS_PER_CHANNEL )

/* pixel unit - a single colour channel */
typedef uint8_t CHANNEL;

void
next ( cimg_library::CImg<CHANNEL> *img, int &pix, int &channel ) {
    channel = ( (channel) + 1) % img->spectrum();
    if(!(channel)) {
        (pix)++;
    }
}

/* embed a unit of information starting at the specified location in the
 * image. This function will update the pix and channel ints to point to the
 * location just after the embedded information after the operation is 
 * complete */
void
embed ( cimg_library::CImg<CHANNEL> *img, uint64_t data, size_t bytes,
        int &pix, int &channel ) {

    /* compute how many channels we'll need to store the data and begin to 
     * iterate over them, storing as necessary */
    for( unsigned int i=0; i<CHANNELS_TO_ENCODE(bytes); i++) {
        /* retrieve the next channel to be used for encoding from the image */
        CHANNEL *p = img->data( pix%img->width(), pix/img->width(), 0, 
                channel );
        
        /* clear the target bits of the image to remove any information
         * that is already stored there */        
        *p &= ~CHANNEL_BIT_MASK;
        
        /* now embed the required number of bits in the cleared pixel channel
         * bits */
        *p |= (data >> ((CHANNELS_TO_ENCODE(bytes)-1-i)*
                    ENCODE_BITS_PER_CHANNEL)) & CHANNEL_BIT_MASK;
        
        /* update pix and channel to be the indexes of the next pixel/channel
         * combination of interest */
        next( img, pix, channel );
    }    
}

/* retrieve a unit of information from the specified location in the encoded
 * image. The function will update the values of pix and channel to point to
 * the location just after the end of the retrieved data's location */
uint64_t
retrieve ( cimg_library::CImg<uint8_t> *img, size_t bytes, int &pix, 
        int &channel ) {

    uint64_t c = 0;

    for( unsigned int i=0; i<CHANNELS_TO_ENCODE(bytes); i++ ) {
        /* retrieve a channel containing information we need to extract */
        CHANNEL *p = img->data( pix%img->width(), pix/img->width(), 0, 
                channel );
        
        /* extrct the encoded bits from the channel and merge with a running
         * tally of bits */
        c = (c << ENCODE_BITS_PER_CHANNEL) | (*p & CHANNEL_BIT_MASK);
        
        /* update pix and channel to be the indexes of the next pixel/channel
         * combination of interest */
        next( img, pix, channel );
        
    } 

    /* return the retrieved data */
    return c;  
}

void
embed_file_in_image( std::ifstream &file, std::string filename, 
        cimg_library::CImg<CHANNEL> *img ) {
    
    int pix=0, channel=0;
    
    /* compute the size of the file */
    std::streampos fsize = 0;
    fsize = file.tellg();
    file.seekg( 0, std::ios::end );
    fsize = file.tellg() - fsize;
    file.seekg(0, std::ios::beg);
   
    /* embed the filename and the filename length in the image */
    embed( img, filename.length(), sizeof(uint8_t), pix, channel );
    for( int i=0; i<filename.length(); i++ ) {
        embed( img, filename[i], sizeof(uint8_t), pix, channel );
    }

    /* embed file size in the image */
    embed( img, fsize, sizeof(uint64_t), pix, channel );

    /* embed file data in the image */
    uint8_t byte;
    while( !file.eof() ) {
        byte = file.get();
        embed( img, byte, sizeof(uint8_t), pix, channel );
    } 
}

void
retrieve_file_from_image( cimg_library::CImg<CHANNEL> *img ) {
    int pix=0, channel=0;
    
    std::ofstream out;
    uint64_t fsize;
    uint8_t  fname_len;
    std::string fname;

    /* retrieve information about the file we are about to load - file size
     * file name and the length of the file name */
    fname_len = retrieve( img, sizeof(uint8_t), pix, channel );
    for( int i=0; i<fname_len; i++ ) {
        char c = retrieve( img, sizeof(char), pix, channel );
        fname += c;
    }
    fsize = retrieve( img, sizeof(uint64_t), pix, channel );

    /* open the target output file for writing */
    out.open(fname.c_str(), std::ios::binary);
    if(!out.is_open()) {
        /* handle case where we can't open the file for some reason */
        std::cout << "Unable to open " << fname << " for writing" << std::endl;
        return;
    }

    /* start retrieving file data from the image and writing to the output
     * stream */
    for( uint64_t i=0; i<fsize; i++ ) {
        uint8_t byte = retrieve( img, sizeof(uint8_t), pix, channel );
        out << byte;
    }

    /* close the output stream now that we are done */
    out.close();
}

int
main ( int argc, char *argv[] )
{
    /*
    std::ifstream in;
    cimg_library::CImg<CHANNEL> img;

    if( argc != 3 ) {
        std::cout << "usage: steg FILE IMG" << std::endl;
        return EXIT_FAILURE;    
    }

    in.open(argv[1], std::ios::binary);
    if(!in.is_open()) {
        std::cout << "unable to open " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }

    try {
        img.load(argv[2]);
    } catch ( cimg_library::CImgIOException e ) {
        return EXIT_FAILURE;
    }

    std::cout << "ENCODING... ";
    std::cout.flush();
    embed_file_in_image( in, "test.txt", &img);
    std::cout << "OK" << std::endl;
    
    img.save("out.png");
*/
    cimg_library::CImg<CHANNEL> img;

    if( argc != 2 ) {
        std::cout << "usage: steg IMG" << std::endl;
        return EXIT_FAILURE;    
    }

    try {
        img.load(argv[1]);
    } catch ( cimg_library::CImgIOException e ) {
        return EXIT_FAILURE;
    }

    retrieve_file_from_image( &img);

    return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
