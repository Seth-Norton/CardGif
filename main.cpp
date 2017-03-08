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
#include <time.h>
#include <stdint.h>

#define W 500      //Just for setting framebuffer size
#define H 500      //without complaints about variable size

#define DATASIZE 9

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
 * Thank you Andrew Kensler for this elegant vector setup.
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
    vec operator-(vec b){
        return vec(x-b.x,y-b.y,z-b.z,w-b.z);
    }
    vec operator*(float b){
        return vec(x*b,y*b,z*b,w*b);
    }
	vec operator*(vec b){
		return vec(x*b.x, y*b.y, z*b.z, w*b.w);
	}
    float operator%(vec b){
        return x*b.x+y*b.y+z*b.z+w*b.w;
    }
    vec operator^(vec b){ // Dot product that assumes we're only using three dimensions
        return vec(y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x, 0);
    }
    vec operator!(){
        return *this*(1 /sqrt(*this%*this));
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


void writeFrameSpeedControl(){
	outfile << (char)33;
	outfile << (char)249;
	outfile << (char)4;
	outfile << (char)8;
	outfile << (char)3;	//	Pause in 1/10 second, 30 FPS
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
	writeFrameSpeedControl();
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
    view[2]=vec(0, 0, 1, 5);
    view[3]=vec(0, 0, 0, 1);

    persp[0]=vec(1.0f      , 0          , 0           , 0);
    persp[1]=vec(0         ,-1.0f       , 0           , 0);
    persp[2]=vec(0         , 0          , 101.0f/99.0f,-200.0f/99.0f);
    persp[3]=vec(0         , 0          , 1           , 0);
}

void rot(float deg){
    setView();
    deg=deg*3.14159/180.0;
    view[0]=vec(cos(deg),0,sin(deg),0);
    view[1]=vec(0,1,0,0);
	view[2]=vec(-sin(deg),0,cos(deg),5);
}

/*
 * We'll replace this later with procedural triangles.
 */
void makeData(){
    points[0]=vec(0.0f , .7f,0.0f,1.0f);
    points[1]=vec(-1.0f,-.7f,0.0f,1.0f);
    points[2]=vec(1.0f ,-.7f,0.0f,1.0f);

    points[3]=vec(1.0f , 1.7f,3.0f,1.0f);
    points[4]=vec(0.0f,-1.7f,2.0f,1.0f);
    points[5]=vec(2.0f ,-1.7f,2.0f,1.0f);

    points[6]=vec(0.0f , .7f,0.0f,1.0f);
    points[7]=vec(-1.0f,-.7f,0.0f,1.0f);
    points[8]=vec(1.0f ,-.7f,0.0f,1.0f);
}






//---   IMAGE PREPARATION FUNCTIONS   ---//
/*
 * Provides pure black slate to draw on
 */
void clearFrameBuf(){
    memset(frameBuf, backgroundColor, width*height);
}

vec project(vec v){
    vec ret = matrixMult(view, v);
    ret = matrixMult(persp, ret);
    if(ret.w > 0)
        ret = ret*(1.0f/ret.w);   // Correct for perspective

    return ret;
}

/*
 * Returns: false if outside triangle,
 *          true if within triangle
 */
bool sameSide(vec p1, vec p2, vec a, vec b){
    vec cp1 = (b-a)^(p1-a);
    vec cp2 = (b-a)^(p2-a);
    if(cp1%cp2 >= 0)
        return true;
    return false;
}

float withinTriangle(int x, int y, vec v1, vec v2, vec v3){
    vec pix(((double)x)/width, ((double)y)/height, v1.z, 0);

    if(sameSide(pix, v1, v2, v3) &&
       sameSide(pix, v2, v1, v3) &&
       sameSide(pix, v3, v1, v2))
        return 1;
    return 0;
}


/*
 * Given three points, rasterize() will draw the triangle to the buffer.
 */
void rasterize(vec v1, vec v2, vec v3, vec nor){
    vec light(0,0,1,0);
    light = !light;
    vec bot, top;
    bot.x = min(min(v1.x, v2.x), v3.x);
    bot.y = min(min(v1.y, v2.y), v3.y);

    top.x = max(max(v1.x, v2.x), v3.x);
    top.y = max(max(v1.y, v2.y), v3.y);

    bot.x *= width;
    bot.y *= height;
    top.x *= width;
    top.y *= height;
	
	bot.x += width/2;
	bot.y += height/2;
	top.x += width/2;
	top.y += height/2;
	
	
	//	Counting on floating point math to avoid dividing by 0
	double slope1 = (v1.y-v2.y)/(v1.x-v2.x);
	double slope2 = (v1.y-v3.y)/(v1.x-v3.x);
	double slope3 = (v3.y-v2.y)/(v3.x-v2.x);
	
	



    int col = (int)(126*(nor%light));
    if(col < 0)
        col = 0;
	
	
	

    for(int y=top.y; y>bot.y; y--)
        if(y > 0 && y < height){    // Early escape if y is out of frame
			//	Find entry and exit points for this scanline
			
			double ent1, ent2, ent3;
			double ext1, ext2, ext3;
			
			if((float)slope1 != 0){
				ent1 = v1.x*width + width/2 + (y-(v1.y*height + height/2))/slope1;
				ext1 = ent1;
				
				if(ent1 < bot.x)
					ent1 = width;
				if(ext1 > top.x)
					ext1 = 0;
			}
			else{
				if(y == v1.y*height + height/2){
					ent1 = bot.x;
					ext1 = top.x;
				}
				else{
					ent1 = width;
					ext1 = 0;
				}
			}
			
			if((float)slope2 != 0){
				ent2 = v1.x*width + width/2 + (y-(v1.y*height + height/2))/slope2;
				ext2 = ent2;
				
				if(ent2 < bot.x)
					ent2 = width;
				if(ext2 > top.x)
					ext2 = 0;
			}
			else{
				if(y == v1.y*height + height/2){
					ent2 = bot.x;
					ext2 = top.x;
				}
				else{
					ent2 = width;
					ext2 = 0;
				}
			}
			
			if((float)slope3 != 0){
				ent3 = v3.x*width + width/2 + (y-(v3.y*height + height/2))/slope3;
				ext3 = ent3;
				
				if(ent3 < bot.x)
					ent3 = width;
				if(ext3 > top.x)
					ext3 = 0;
			}
			else{
				if(y == v3.y*height + height/2){
					ent3 = bot.x;
					ext3 = top.x;
				}
				else{
					ent3 = width;
					ext3 = 0;
				}
			}
			
			
			int entry = min(min(ent1, ent2), ent3);
			int exit = max(max(ext1, ext2), ext3);
	
	
            for(int x=entry; x<exit; x++)
                if(x > 0 && x < width){
					double coverage = 1;
					if(x == entry){
						coverage = 1-(min(min(ent1, ent2), ent3) - (double)x);
					}
					else if(x == exit-1){
						coverage = max(max(ext1, ext2), ext3) - (double)x-1;
					}
	
                    frameBuf[y*width+x] = min(col*coverage+frameBuf[y*width+x]*(1-coverage), (double)255);
                }
				
		}
}

/*
 * For each triangle, we need to fill every point within that triangle.
 * We will currently be ignoring the depth check normally done upon rasterizing.
 */
void render(){
    clearFrameBuf();
    for(int i=0; i<DATASIZE-2; i+=3){
        vec v1, v2, v3;
        v1 = project(points[i]);
        v2 = project(points[i+1]);
        v3 = project(points[i+2]);

        vec nor = !((points[i+1]-points[i])^(points[i+2]-points[i]));

        rasterize(v1, v2, v3, nor);
    }
	
	
	//	DEBUG: Draw image borders
	for(int i = 0; i < width; i++){
		frameBuf[i] = 100;
		frameBuf[(height-1)*width+i] = 100;
		
		frameBuf[i*width] = 100;
		frameBuf[i*width+width-1] = 100;
	}
	
}


//---  MAIN LOGIC  ---//
int main()
{
    setView();
    makeData();
    clock_t startT, processT = 0, writeT = 0;

    writeHeader();                      // Set up file
    writeGCT();                         // |
    writeForceLoop();                   // |
                                        // |
    //  Do every third degree because writing to disk is painfully slow
    for(float i=0; i<360; i+=3){        // |
        cout << "Deg: " << i << endl;   // |


        startT = clock();               // |
        render();                       // |
        rot(i);                         // |
        processT += clock()-startT;     // |


        startT = clock();               // |
        writeFrameBuf();                // |
        writeT += clock()-startT;       // |
    }                                   // |
                                        // |
                                        // |
    writeEndFile();                     // Finish file

    cout << "Processing: " << processT/1000 << " ms" << endl;
    cout << "Writing:    " << writeT/1000 << " ms" << endl;
    return 0;
}
