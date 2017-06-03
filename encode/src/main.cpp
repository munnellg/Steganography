#include "CImg.h"
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <stdio.h>

#define TERMINATE_BYTE_LO 0x5D
#define TERMINATE_BYTE_HI 0xCF

void
next( int *pix, int *channel ) {
    *channel = ( (*channel) + 1) % 3;
    if(!(*channel)) {
        (*pix)++;
    }
}

void
embed_byte( cimg_library::CImg<uint8_t> *img, char c ) {
    static int pix=0, channel=0;
    
    for( int i=0; i<4; i++ ) {
        uint8_t *p = img->data( pix%img->width(), pix/img->width(), 0, 
                channel );
        
        *p &= 0xFC;
        *p |= (c >> ((3-i)*2)) & 0x3;
        
        next( &pix, &channel );
    }    
}

char
retrieve_char( cimg_library::CImg<uint8_t> *img ) {
    uint8_t c = 0;
    static int pix=0, channel=0;

    for( int i=0; i<4; i++ ) {
        uint8_t *p = img->data( pix%img->width(), pix/img->width(), 0, 
                channel );
        
        c = (c << 2) | (*p & 0x3);
        
        next( &pix, &channel );
    }
    return c;  
}

int
main ( int argc, char *argv[] )
{
    uint8_t byte;
    std::ifstream in;
    cimg_library::CImg<uint8_t> img;
    std::string message;

    if( argc != 4 ) {
        std::cout << "usage: steg FILE IN OUT" << std::endl;
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
    
    while( !in.eof() ) {
        byte = in.get();
        embed_byte( &img, byte );
    }
    embed_byte( &img, TERMINATE_BYTE_LO ); 
    embed_byte( &img, TERMINATE_BYTE_HI );
    std::cout << "OK" << std::endl;
    
    img.save(argv[3]);

    return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
