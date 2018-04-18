#include <stdio.h>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

template <typename T>
inline T SampleOrZero(T* data, size_t width, size_t height, int x, int y)
{
	if (x < 0 || y < 0 || x > width - 1 || y > height - 1)
		return T(0);

	return data[y*width + x];
}

void BoxBlur(const uint8* source, size_t width, size_t height, size_t radius, const char* baseFileName)
{
	std::vector<uint8> result;
	result.resize(width * height);

	float filterSize = float(radius * 2 + 1)*float(radius * 2 + 1);

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			float sum = 0;

			for (int filtery = -(int)radius; filtery <= (int)radius; ++filtery)
			{
				for (int filterx = -(int)radius; filterx <= (int)radius; ++filterx)
				{
					sum += (float)SampleOrZero(source, width, height, ix + filterx, iy + filtery);
				}
			}

			result[iy*width + ix] = uint8(0.5f + sum / filterSize);
		}
	}

	char append[32];
	sprintf_s(append, "_%i", int(radius));
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, (int)width, (int)height, 1, &result[0], (int)width);
}

void SATBoxBlur(const std::vector<uint32>& SAT, size_t width, size_t height, size_t radius, const char* baseFileName)
{
	std::vector<uint8> result;
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

			double size = double((endY - startY)*(endX - startX));

			uint8 average = uint8(double(integratedValue) / size);

			result[iy*width + ix] = average;
		}
	}

	char append[32];
	sprintf_s(append, "_%i_SAT", int(radius));
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, (int)width, (int)height, 1, &result[0], (int)width);
}

void AATBoxBlur(const std::vector<uint8>& AAT, const std::vector<uint32>& SAT, size_t width, size_t height, size_t radius, const char* baseFileName)
{
	// TODO: remove SAT parameter

	std::vector<uint8> result;
	result.resize(AAT.size());

	for (size_t iy = 0; iy < height; ++iy)
	{
		for (size_t ix = 0; ix < width; ++ix)
		{
			size_t startX = (ix < radius + 1) ? 0 : ix - radius - 1;
			size_t startY = (iy < radius + 1) ? 0 : iy - radius - 1;

			size_t endX = std::min(ix + radius, width - 1);
			size_t endY = std::min(iy + radius, height - 1);

			uint32 A = uint32(AAT[startY*width + startX]) * uint32((startY + 1)*(startX + 1));
			uint32 B = uint32(AAT[startY*width + endX]) * uint32((startY + 1)*(endX + 1));
			uint32 C = uint32(AAT[endY*width + startX]) * uint32((endY + 1)*(startX + 1));
			uint32 D = uint32(AAT[endY*width + endX]) * uint32((endY + 1)*(startY + 1));

			uint32 SATA = SAT[startY*width + startX];
			uint32 SATB = SAT[startY*width + endX];
			uint32 SATC = SAT[endY*width + startX];
			uint32 SATD = SAT[endY*width + endX];

			uint32 integratedValue = A + D - B - C;

			double size = double((endY - startY)*(endX - startX));

			uint8 average = uint8(double(integratedValue) / size);

			result[iy*width + ix] = average;
		}
	}

	char append[32];
	sprintf_s(append, "_%i_AAT", int(radius));
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, (int)width, (int)height, 1, &result[0], (int)width);
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

	size_t radiuses[] = { 1, 5, 25 };

	for (size_t index = 0; index < _countof(radiuses); ++index)
	{
		// regular box blur
		//BoxBlur(source, width, height, radiuses[index], baseFileName);

		// box blur with SAT
		//SATBoxBlur(SAT, width, height, radiuses[index], baseFileName);

		// box blur with AAT
		AATBoxBlur(AAT, SAT, width, height, radiuses[index], baseFileName);
	}
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
* fix box blur edges to look right. eg for regular box blur, maybe divide by the number of pixels that are in range, instead of by filter size always (dont assume it's black around border?).
 * that would make it like the SAT one

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