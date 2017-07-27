#include "CImg.h"
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <sstream>
#include <climits>
#include <boost/filesystem.hpp>
#include <map>

/* some helpful macros for determining things like the number of pixel channels
 * required to encode a unit of information or masks for clearing/setting bits
 * during encoding/retrieval */
#define BYTES_TO_BITS(x)        ((x) * CHAR_BIT)
#define NUM_CHANNEL_BITS        BYTES_TO_BITS(sizeof(CHANNEL))
#define ENCODE_BITS_PER_CHANNEL 2
#define CHANNEL_BIT_MASK        ((((uint64_t) 1) << ENCODE_BITS_PER_CHANNEL)-1)
#define CHANNELS_TO_ENCODE(x)   ( BYTES_TO_BITS((x))/ENCODE_BITS_PER_CHANNEL )

const char* DEFAULT_OUTPUT = "out.png";

/* operating modes of the program */
enum Mode { EMBED, DECODE, SUBTRACT };
enum ArgKey { IMAGE, EMBED_FILE, OUTPUT_FILE, SUBTRACT_FILE };

typedef uint8_t  BYTE;    /* 8 bit unsigned integer */
typedef uint64_t LONG;    /* 64 bit unsigned integer */
typedef char     CHAR;    /* single string character */
typedef uint8_t  CHANNEL; /* pixel unit - a single colour channel */

typedef std::map <ArgKey,char*> ArgMap;

/* decode an image to a file by default */
Mode g_mode = DECODE;

void
usage() {
    std::cout<< 
        "usage: steg [ -e FILE | -o FILE | -s IMAGE2 ] IMAGE" << std::endl
        << std::endl 
        << "-e embed FILE in IMAGE" << std::endl
        << "-o output result to FILE" << std::endl
        << "-s subtract IMAGE2 from IMAGE" << std::endl;
    exit(-1);
}

void
warn( std::string message ) {
    std::cout << "WARNING: " << message << std::endl;
}

void
die( std::string message ) {
    std::cout << "ERROR: " << message << std::endl;
    exit(-1);
}

void
next ( cimg_library::CImg<CHANNEL> *img, int &pix, int &channel ) {
    /* choose next channel from those available */
    channel = (channel + 1) % img->spectrum();
    /* advance to next pixel if required */
    if(!channel) {
        pix++;
    }
}

/* embed a unit of information starting at the specified location in the
 * image. This function will update the pix and channel ints to point to the
 * location just after the embedded information once the operation is 
 * complete */
void
embed ( cimg_library::CImg<CHANNEL> *img, LONG data, size_t bytes,
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
 * the location just after the retrieved data's location */
LONG
retrieve ( cimg_library::CImg<CHANNEL> *img, size_t bytes, int &pix, 
        int &channel ) {

    LONG c = 0;

    for( LONG i=0; i<CHANNELS_TO_ENCODE(bytes); i++ ) {
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
subtract_images ( cimg_library::CImg<CHANNEL> &img,  
        cimg_library::CImg<CHANNEL> &sub,
        cimg_library::CImg<CHANNEL> &result ) {
   
    unsigned int max = img.max(); 
    for(int x=0; x<img.width(); x++) {
        for(int y=0; y<img.height(); y++) {
            for( int s=0; s<img.spectrum(); s++ ) {
                CHANNEL *p1 = img.data( x, y, 0, s );
                CHANNEL *p2 = sub.data( x, y, 0, s );
                CHANNEL *p3 = result.data( x, y, 0, s );
                
                /* Normalize the difference based on the largest pixel value
                 * in the image */
                *p3 = (((double)abs(*p1 - *p2))/CHANNEL_BIT_MASK)*max;
            }
        }
    }
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

    /* strip away any path information from our filename in a platform
     * undependent way */
    boost::filesystem::path p(filename);
    filename = p.filename().string();

    /* before we start writing data to the image, make sure it is large
     * enough to store the embedded data. An image has a capacity that is 
     * equal to its area times the number of channels per pixel times the
     * number of bits we are storing per pixel. This yields a capacity in
     * bits. We divide by the size of a byte (our smallest unit of storage)
     * to get the capacity in bytes. This is compared to the size of the file
     * plus the bits of metadata we include for pulling the data back out.
     * If requirements exceed available resources then the program fails */
    LONG embed_size = sizeof(BYTE) + filename.length() + sizeof(LONG) + fsize;
    LONG image_capacity = 
        (img->width() * img->height() * img->spectrum() * 
         ENCODE_BITS_PER_CHANNEL)/(BYTES_TO_BITS(sizeof(BYTE)));

    if( embed_size > image_capacity ) {
        die("Image not large enough to embed data");
    }

    /* embed the filename and the filename length in the image */
    embed( img, filename.length(), sizeof(BYTE), pix, channel );
    for( LONG i=0; i<filename.length(); i++ ) {
        embed( img, filename[i], sizeof(CHAR), pix, channel );
    }

    /* embed file size in the image */
    embed( img, fsize, sizeof(LONG), pix, channel );

    /* embed file data in the image */
    BYTE byte;
    while( !file.eof() ) {
        byte = file.get();
        embed( img, byte, sizeof(BYTE), pix, channel );
    } 
}

void
retrieve_file_from_image( cimg_library::CImg<CHANNEL> *img, 
        char *output_name = NULL ) {
    int pix=0, channel=0;
    
    std::ofstream out;
    uint64_t fsize;
    uint8_t  fname_len;
    std::string fname;

    /* retrieve information about the file we are about to load - file size
     * file name and the length of the file name */
    fname_len = retrieve( img, sizeof(BYTE), pix, channel );
    for( int i=0; i<fname_len; i++ ) {
        char c = retrieve( img, sizeof(CHAR), pix, channel );
        fname += c;
    }
    fsize = retrieve( img, sizeof(LONG), pix, channel );

    /* open the target output file for writing */
    out.open((output_name)? output_name : fname.c_str(), std::ios::binary);
    if(!out.is_open()) {
        /* handle case where we can't open the file for some reason */
        std::cout << "Unable to open " << fname << " for writing" << std::endl;
        return;
    }

    /* start retrieving file data from the image and writing to the output
     * stream */
    for( LONG i=0; i<fsize; i++ ) {
        BYTE byte = retrieve( img, sizeof(BYTE), pix, channel );
        out << byte;
    }

    /* close the output stream now that we are done */
    out.close();
}

/* handles input arguments from the command line. Extracts target file names,
 * sets up the programs mode of operation and any global configuration options
 * which the user has deigned to change */
ArgMap
handle_args ( int argc, char *argv[] ) {
    ArgMap args;
    for( int i=1; i<argc; i++ ) {
        /* any argument which begins with a dash is presumed to be a flag. We
         * parse the flag as appropriate */
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'e':
                    /* the e flag sets up the program to run in embed mode. It
                     * also expects that the user has passed an input file to
                     * be embedded. If no argument follows the -e flag then
                     * the user has neglected to specify the file they wish
                     * to embed and the program's operation terminates */
                    if(i+1 >= argc) {
                        std::ostringstream oss;
                        oss << argv[i] << " expects an argument";
                        die(oss.str());
                    }
                    
                    i++;
                    g_mode = EMBED;
                    args[EMBED_FILE] = argv[i];
                    break;
                case 'o':
                    /* the o flag specifies the name of the output file. If 
                     * running in decode mode, then this will be the name of 
                     * the file extracted from the image. If running in embed
                     * mode then this will be the name of the output image
                     * produced by the program. If no argument follows the -o
                     * flag then the user has neglected to specify an output
                     * file name and a warning will be issued. The program 
                     * will then proceed to use default output names */
                    if(i+1 >= argc) {
                        std::ostringstream oss;
                        oss << argv[i] << " expects an argument";
                        warn(oss.str());
                    } else {
                        i++;
                        args[OUTPUT_FILE] = argv[i];
                    }             
                    break;
                case 's':
                    /* the s flag sets up the program to run in subtract mode. 
                     * It also expects that the user has passed an input file 
                     * to be subtracted. If no argument follows the -s flag 
                     * then the user has neglected to specify the file they 
                     * wish to subtract and the program's operation 
                     * terminates */
                    if(i+1 >= argc) {
                        std::ostringstream oss;
                        oss << argv[i] << " expects an argument";
                        die(oss.str());
                    }
                    
                    i++;
                    g_mode = SUBTRACT;
                    args[SUBTRACT_FILE] = argv[i];
                    break;
                default:
                    /* user tried to use a flag that the program does not
                     * support. Issue a warning and proceed */
                    std::ostringstream oss;
                    oss << "Invalid flag: " << argv[i];
                    warn(oss.str());
            }            
        } else {
            /* any argument which does not begin with a dash (and is not an
             * argument to some other existing flag)  must be an input file to 
             * the program. At present this should really only be the input 
             * image file, but in future there could be more */
            ArgMap::iterator it = args.find(IMAGE);
            if(it != args.end()) {
                std::ostringstream oss;
                oss << "Already have input image. Ignoring " << argv[i] << 
                    " and continuing with " << it->second;
                warn(oss.str());
            } else {
                args[IMAGE] = argv[i];
            }
        }
    }
    return args;
}


void
run_embed_mode( ArgMap args ) {
    char *image_name;
    char *filename;
    char *output_name;

    ArgMap::iterator it = args.find(IMAGE);
    if(it == args.end()) {
        usage();
    } 

    image_name = it->second;

    it = args.find(EMBED_FILE);
    if(it == args.end()) {
        usage();
    } 

    filename = it->second;

    it = args.find(OUTPUT_FILE);
    if(it == args.end()) {
        output_name = const_cast<char*> (DEFAULT_OUTPUT);
    } else {
        output_name = it->second;
    }

    std::ifstream in;
    cimg_library::CImg<CHANNEL> img;

    in.open(filename, std::ios::binary);
    if(!in.is_open()) {
        std::ostringstream oss;
        oss << "unable to open " << filename;
        die(oss.str());
    }

    try {
        img.load(image_name);
    } catch ( cimg_library::CImgIOException e ) {
        std::ostringstream oss;
        oss << "unable to open " << image_name;
        die(oss.str());
    }

    embed_file_in_image( in, filename, &img);

    img.save(output_name);

    in.close();
}

void
run_decode_mode( ArgMap args ) {
    char *image_name;
    char *output_name;

    ArgMap::iterator it = args.find(IMAGE);
    if(it == args.end()) {
        usage();
    } 

    image_name = it->second;

    it = args.find(OUTPUT_FILE);
    if(it == args.end()) {
        output_name = NULL;
    } else {
        output_name = it->second;
    }

    cimg_library::CImg<CHANNEL> img;
    try {
        img.load(image_name);
    } catch ( cimg_library::CImgIOException e ) {
        std::ostringstream oss;
        oss << "unable to open " << image_name;
        die(oss.str());
    }

    retrieve_file_from_image( &img, output_name );
}

void
run_subtract_mode( ArgMap args ) {
    char *image_name;
    char *subtract_name;
    char *output_name;

    ArgMap::iterator it = args.find(IMAGE);
    if(it == args.end()) {
        usage();
    } 

    image_name = it->second;

    it = args.find(SUBTRACT_FILE);
    if(it == args.end()) {
        usage();
    } 

    subtract_name = it->second;

    it = args.find(OUTPUT_FILE);
    if(it == args.end()) {
        output_name = const_cast<char*> (DEFAULT_OUTPUT);
    } else {
        output_name = it->second;
    }

    cimg_library::CImg<CHANNEL> img;
    try {
        img.load(image_name);
    } catch ( cimg_library::CImgIOException e ) {
        std::ostringstream oss;
        oss << "unable to open " << image_name;
        die(oss.str());
    }

    cimg_library::CImg<CHANNEL> sub;
    try {
        sub.load(subtract_name);
    } catch ( cimg_library::CImgIOException e ) {
        std::ostringstream oss;
        oss << "unable to open " << subtract_name;
        die(oss.str());
    }

    if( img.width() != sub.width() || img.height() != sub.height() || 
            img.spectrum() != sub.spectrum() || img.depth() != sub.depth() ) {
        die("Subtraction requires two images of the same size");
    }

    cimg_library::CImg<CHANNEL> result( img.width(), img.height(), img.depth(), 
            img.spectrum(), 0);

    subtract_images( img, sub, result );

    result.save(output_name);
}

int
main ( int argc, char *argv[] )
{
    ArgMap args = handle_args( argc, argv );

    switch(g_mode) {
        case EMBED:
            run_embed_mode(args);
            break;
        case DECODE:
            run_decode_mode(args); 
            break;
        case SUBTRACT:
            run_subtract_mode(args);
            break;
    }

    return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
