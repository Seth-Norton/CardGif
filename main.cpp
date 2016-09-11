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

#include <iostream>
#include <fstream>  //For output file stream
#include <string.h> //For memset()
#include <math.h>   //For sin/cos

#define W 100      //Just for setting framebuffer size
#define H 100      //without complaints about variable size

#define DATASIZE 3

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


//---   3D DATA   ---//

/*
 * Thank you Paul Heckbert for this elegant vector setup.
 */
struct vec{
    float x,y,z,w;
    vec(){}
    vec(float a, float b, float c, float d){
        x=a;
        y=b;
        z=c;
        w=d;
    }
    vec operator+(vec b){
        return vec(x+b.x,y+b.y,z+b.z,w+b.z);
    }
    vec operator*(float b){
        return vec(x*b,y*b,z*b,w*b);
    }
    float operator%(vec b){
        return x*b.x+y*b.y+z*b.z+w*b.w;
    }
};

vec view[4];
vec persp[4];
vec points[DATASIZE];



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


//---   3D DESCRIPTION FUNCTIONS   ---//
vec matrixMult(vec* a, vec b){
    return(vec(a[0]%b, a[1]%b, a[2]%b, a[3]%b));
}

/*
 * Translates scene to z=3.
 * Perspective is a 90 degree frustum with z-near=1, z-far=100
 */
void setView(){
    view[0]=vec(1, 0, 0, 0);
    view[1]=vec(0, 1, 0, 0);
    view[2]=vec(0, 0, 1, 3);
    view[3]=vec(0, 0, 0, 1);

    persp[0]=vec(1.0f      , 0          , 0           , 0);
    persp[1]=vec(0         ,-1.0f, 0           , 0);
    persp[2]=vec(0         , 0          , 101.0f/99.0f,-200.0f/99.0f);
    persp[3]=vec(0         , 0          , 1           , 0);

    //  Debug   //
    /*cout << "-- View matrix --" << endl;
    for(int i=0; i<4; i++){
        cout << view[i].x << " " << view[i].y << " "
             << view[i].z << " " << view[i].w << " ";
        cout << endl;
    }
    cout << endl << "-- Perspective matrix --" << endl;
    for(int i=0; i<4; i++){
        cout << persp[i].x << " " << persp[i].y << " "
             << persp[i].z << " " << persp[i].w << " ";
        cout << endl;
    }

    vec pos(3, 0, 0, 1);
    cout << endl << "-- Pre-Translation --" << endl;
    cout << pos.x << " " << pos.y << " "
         << pos.z << " " << pos.w << " ";
    cout << endl;

    vec temp = matrixMult(view, pos);
    temp = matrixMult(persp, temp);
    temp = temp*(1.0f/temp.w);
    cout << "-- Translated --" << endl;
    cout << temp.x << " " << temp.y << " "
         << temp.z << " " << temp.w << " ";
    cout << endl;*/
}

void rot(float deg){
    setView();
    deg=deg*3.14159/180.0;
    view[0]=vec(cos(deg),-sin(deg),0,0);
    view[1]=vec(sin(deg),cos(deg),0,0);
}

/*
 * We'll replace this later with procedural triangles.
 */
void makeData(){
    points[0]=vec(0.0f , .7f,0.0f,1.0f);
    points[1]=vec(-1.0f,-.7f,0.0f,1.0f);
    points[2]=vec(1.0f ,-.7f,0.0f,1.0f);
}






//---   IMAGE PREPARATION FUNCTIONS   ---//
/*
 * Provides pure black slate to draw on
 */
void clearFrameBuf(){
    memset(frameBuf, backgroundColor, width*height);
}


/*
 * It may not be pretty, but now we take points
 * and put them as 20-pixel-wide horizontal lines.
 * Seeing as the points are in a nice line themselves,
 * it will make a thick line across the screen.
 */
void render(){
    clearFrameBuf();
    for(int i=0; i<DATASIZE; i++){
        vec v;
        v = matrixMult(view, points[i]);
        v = matrixMult(persp, v);
        if(v.w > 0)
            v = v*(1.0f/v.w);   // Correct for perspective

        int lineWidth = 20;
        for(int j=0; j<lineWidth; j++){
            if(v.y < 1.0f && v.x < 1.0f &&
               v.y >-1.0f && v.x >-1.0f){
                vec pix(v.x*width, v.y*height, v.z, v.w);
                frameBuf[((int)pix.y*width)+((height/2)*width)+(int)pix.x+j-lineWidth/2+width/2] = 126;
            }
            else{
                cout << "Culled" << endl;
            }
        }
    }
}


//---  MAIN LOGIC  ---//
int main()
{
    setView();
    makeData();

    writeHeader();  //Set up file
    writeGCT();
    writeForceLoop();

    //  Do every third degree because writing to disk is painfully slow
    for(int i=0; i<360; i+=3){
        cout << "Deg: " << i << endl;
        render();
        writeFrameBuf();
        rot((float)i);
    }


    writeEndFile(); //Finish file
    return 0;
}
