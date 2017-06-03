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

char
retrieve_byte( cimg_library::CImg<uint8_t> *img ) {
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
    uint8_t terminate[] = { TERMINATE_BYTE_LO, TERMINATE_BYTE_HI };
    uint8_t buffer[2] = {0};
    std::ofstream out;
    cimg_library::CImg<uint8_t> img;
    std::string message;

    if( argc != 3 ) {
        printf("usage: steg FILE IMAGE");
        return EXIT_FAILURE;    
    }

    out.open(argv[1], std::ios::binary);
    if(!out.is_open()) {
        std::cout << "unable to open " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }

    try {
        img.load(argv[2]);
    } catch ( cimg_library::CImgIOException e ) {
        return EXIT_FAILURE;
    }

    std::cout << "DECODING... ";
    std::cout.flush();
   
    buffer[1] = retrieve_byte( &img ); 
    while( !out.eof() ) {
        buffer[0] = buffer[1];
        buffer[1] = retrieve_byte(&img);
        if( buffer[0] == terminate[0] && buffer[1] == terminate[1] ) {
            break;
        }
        out << buffer[0];
    }
    std::cout << "OK" << std::endl;

    return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
