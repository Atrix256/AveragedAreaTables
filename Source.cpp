#include <stdio.h>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef uint8_t uint8;
typedef uint32_t uint32;

void SATBoxBlur(const std::vector<uint32>& SAT, size_t width, size_t height, size_t radius, std::vector<uint8>& result)
{
	result.resize(SAT.size());

	for (size_t iy = 0; iy < height; ++iy)
	{
		for (size_t ix = 0; ix < width; ++ix)
		{
			size_t startX = (ix < radius + 1) ? 0 : ix - radius - 1;
			size_t startY = (iy < radius + 1) ? 0 : iy - radius - 1;

			size_t endX = std::min(ix + radius, width - 1);
			size_t endY = std::min(iy + radius, height - 1);

			uint32 A = SAT[startY*width + startX];
			uint32 B = SAT[startY*width + endX];
			uint32 C = SAT[endY*width + startX];
			uint32 D = SAT[endY*width + endX];

			uint32 integratedValue = A + D - B - C;

			uint8 average = uint8(double(integratedValue) / double((endY - startY)*(endX - startX)));

			result[iy*width + ix] = average;
		}
	}
}

void TestAATvsSAT(uint8* source, size_t width, size_t height, const char* baseFileName)
{
    // make SAT
    std::vector<uint32> SAT;
    SAT.resize(width * height);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
			uint32 i_xy = uint32(source[iy*width + ix]);

			uint32 I_xny = (ix > 0) ? SAT[iy*width + (ix - 1)] : 0;
			uint32 I_xyn = (iy > 0) ? SAT[(iy - 1)*width + ix] : 0;
			uint32 I_xnyn = (ix > 0 && iy > 0) ? SAT[(iy - 1)*width + (ix - 1)] : 0;

			SAT[iy*width + ix] = i_xy + I_xny + I_xyn - I_xnyn;
        }
    }

    // make AAT
    std::vector<uint8> AAT;
	AAT.resize(width*height);
    for (size_t iy = 0; iy < height; ++iy)
    {
        for (size_t ix = 0; ix < width; ++ix)
        {
            double value = double(SAT[iy*width + ix]);
			double rangeSize = double((ix + 1)*(iy + 1));
            AAT[iy*width + ix] = uint8((value / rangeSize) + 0.5f);
        }
    }

	char fileName[256];

	// box blur with SAT
	std::vector<uint8> SATBlur1;
	SATBoxBlur(SAT, width, height, 1, SATBlur1);
	sprintf_s(fileName, baseFileName, "_1_SAT");
	stbi_write_png(fileName, (int)width, (int)height, 1, &SATBlur1[0], (int)width);

	std::vector<uint8> SATBlur5;
	SATBoxBlur(SAT, width, height, 5, SATBlur5);
	sprintf_s(fileName, baseFileName, "_5_SAT");
	stbi_write_png(fileName, (int)width, (int)height, 1, &SATBlur5[0], (int)width);

	std::vector<uint8> SATBlur25;
	SATBoxBlur(SAT, width, height, 25, SATBlur25);
	sprintf_s(fileName, baseFileName, "_25_SAT");
	stbi_write_png(fileName, (int)width, (int)height, 1, &SATBlur25[0], (int)width);
}

int main(int argc, char** argv)
{
	// image test
	{
		int width, height, components;
		stbi_uc* pixels = stbi_load("scenery.png", &width, &height, &components, 1);
		TestAATvsSAT(pixels, width, height, "out/scenery%s.png");
		stbi_image_free(pixels);
	}

	// random number test
	{
		std::random_device rd;
		std::mt19937 rng(rd());
		std::uniform_int_distribution<unsigned int> dist(0, 255);

		std::vector<uint8> source;
		source.resize(1024*1024);
		for (uint8& v : source)
			v = dist(rng);

		TestAATvsSAT(&source[0], 1024, 1024, "out/rng%s.png");
	}

	// gradient test? pattern tests?
    
    return 0;
}

/*
TODO:
* SAT box blur doesn't look quite right. probably radius calculation is off or something
* compare to actual box blur of source data!
* and box blur using average area tables
* u8 as source
* show breakdown of float as well? esp f16 if you can.
* try actual images as sources too
* analyze visually (spit out images?), as well as numerically
? is uint32 enough? it should be. log2(1204x1024) = 20, so 28 should be enough for that.

* can add a scaling amount to add precision. aka if you have 2 extra bits to spare, you can multiply everything by 4.
? dither before quantization?
? show 16 bit floats too...http://half.sourceforge.net/

Notes:
* I guess losing fractional bits could be an issue because small fractions over large areas add up to big values

*/