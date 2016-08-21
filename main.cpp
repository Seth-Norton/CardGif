/*
 * ----------   CardGif   ----------
 *  This program will eventually be
 *  compressed to fit on a card.
 *  The goal is to make a pretty GIF
 *  using procedural data.
 *  Currently, it outputs red gradient
 *  stripes and then a blank image so
 *  it flashes.
 *
//  ONLY WORKS ON LITTLE-ENDIAN SYSTEMS
//  NOT SURE ABOUT PORTABILITY YET
 *
 *  Written by: Seth Norton
 *  Started:    Aug. 20, 2016
 */

#include <fstream>  //For output file stream
#include <string.h> //For memset()

#define W 1000      //Just for setting framebuffer size
#define H 1000      //without complaints about variable size

using namespace std;

//---   HEADER INFO   ---//
char signature[] = "GIF89a";
uint16_t width = W;
uint16_t height = H;
uint8_t gctFlag = 1;
uint8_t gctColorRes = 7;
uint8_t gctSortFlag = 0;
uint8_t gctSize = 6;
uint8_t packed1 = (gctFlag<<7)|
                  (gctColorRes<<6)|
                  (gctSortFlag<<3)|
                  (gctSize);
uint8_t backgroundColor = 0;
uint8_t aspectRatio = 0;
//   Packed 1   //
//   7 6 5 4 3 2 1 0
//  +---------------+
//  | |     | |     |
//  +---------------+
//  1 bit - Global color table present flag
//  3 bits- Color resolution
//  1 bit - Sort flag
//  3 bits- Size of global color table


//---   IMAGE START   ---//
char imageDesc = ',';
uint16_t side = 0;
uint16_t top = 0;
uint8_t packed2 = 0;
//   Packed 2   //
//   7 6 5 4 3 2 1 0
//  +---------------+
//  | | | |   |     |
//  +---------------+
//  1 bit - Local color table present flag
//  1 bit - Interlace flag
//  1 bit - Sort flag
//  2 bits- Reserved
//  3 bits- Size of local color table

//---   PIXEL DATA   ---//
uint8_t codeSize = 7;       //The highest order bit is used for clear and stop codes
uint8_t clearTable = 128;   //Highest order bit of the 8 (0x80)
uint8_t stopTable = 129;    //Highest order bit +1
uint8_t scanlineSize = 100; //Must be <= 2^codeSize-3

uint8_t frameBuf[W*H];      //Basically a frame buffer object where we will raster
                            //our image and do any processing before writing to file.

char trailer = ';';         //Should be last byte in file

ofstream outfile ("test.gif", ofstream::binary);



//---   FILE WRITING FUNCTIONS   ---//
/*
 *  Writes header to start file
 */
void writeHeader(){
    outfile << signature;
    outfile.write((char*)(&width), sizeof(width));      //We need both bytes so we use
    outfile.write((char*)(&height), sizeof(height));    //a different writing format
    outfile << packed1;
    outfile << backgroundColor;
    outfile << aspectRatio;

}

/*
 *  Writes global color table
 *  TODO: Change to dynamic size and better output of last byte
 *  TODO: Set more pleasing colors
 */
void writeGCT(){
    for(uint8_t x=0; x<127; x++){
        outfile << x << (char)0 << (char)0;
    }
    outfile << (char)128 << (char)0 << (char)0;
}


void writeForceLoop(){
    //0x21,0xFF,0x0B,"NETSCAPE","2.0",0x03,0x01,0x00,0x00,0x00
    outfile << (char)33;
    outfile << (char)255;
    outfile << (char)11;
    outfile << "NETSCAPE2.0";
    outfile << (char)3;
    outfile << (char)1;
    outfile << (char)0;
    outfile << (char)0;
    outfile << (char)0;

}

/*
 *  Write image description
 */
void writeImageDesc(){
    outfile << imageDesc;
    outfile.write((char*)(&side), sizeof(side));
    outfile.write((char*)(&top), sizeof(top));
    outfile.write((char*)(&width), sizeof(width));
    outfile.write((char*)(&height), sizeof(height));
    outfile << packed2;
}

/*
 * Write end of image
 */
void writeImageEnd(){
    outfile << (char)1;
    outfile << stopTable;
    outfile << (char)0;
}

/*
 * Write the framebuffer data in uncompressed format
 */
void writeFrameBuf(){
    writeImageDesc();
    outfile << codeSize;
    for(uint16_t y=0; y<height; y++){
        for(uint16_t x=0; x<width; x++){

            if(x%scanlineSize == 0){                //Ensure the encodings never go past
                outfile << (char)(scanlineSize+1);  //8 bits by clearing the dictionary
                outfile << clearTable;              //every time we get close.
            }

            outfile << frameBuf[(y*width)+x];   //Send the current pixel out
        }
    }
    writeImageEnd();
}

/*
 *  Writes last byte to say that the animation is done
 */
void writeEndFile(){
    outfile << trailer;
}



//---   IMAGE PREPARATION FUNCTIONS   ---//
/*
 * Soon we will start drawing pictures here
 */
void render(){
    for(uint16_t y=0; y<width; y++){
        for(uint16_t x=0; x<height; x++){
            frameBuf[(y*width)+x] = x&127;
        }
    }
}

/*
 * Provides pure black slate to draw on
 */
void clearFrameBuf(){
    memset(frameBuf, backgroundColor, width*height);
}


//---  MAIN LOGIC  ---//
int main()
{
    writeHeader();  //Set up file
    writeGCT();
    writeForceLoop();

    render();
    writeFrameBuf();//.8 of the frames are the same
    writeFrameBuf();

    clearFrameBuf();//Add a blank frame just to show
    writeFrameBuf();//that it is animating correctly

    render();
    writeFrameBuf();
    writeFrameBuf();


    writeEndFile(); //Finish file
    return 0;
}
