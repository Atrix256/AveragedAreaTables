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

template <typename T>
float AverageOfRectangle(T* data, int width, int height, int sx, int sy, int ex, int ey)
{
    sx = std::min(std::max(sx, 0), width - 1);
    ex = std::min(std::max(ex, 0), width - 1);
    sy = std::min(std::max(sy, 0), height - 1);
    ey = std::min(std::max(ey, 0), height - 1);

    float sum = 0.0f;
    for (int iy = sy; iy <= ey; ++iy)
    {
        for (int ix = sx; ix <= ex; ++ix)
        {
            sum += float(data[iy*width+ix]);
        }
    }

    float sampleCount = float(ey - sy + 1)*float(ex - sx + 1);

    return sum / sampleCount;
}

void BoxBlur(const uint8* source, int width, int height, int radius, const char* baseFileName)
{
	std::vector<uint8> result;
	result.resize(width * height);

	float filterSize = float(radius * 2 + 1)*float(radius * 2 + 1);

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
            float average = AverageOfRectangle(source, width, height, ix - radius, iy - radius, ix + radius, iy + radius);
            result[iy*width + ix] = uint8(0.5f + average);
		}
	}

	char append[32];
	sprintf_s(append, "_%i", int(radius));
	char fileName[256];
	sprintf_s(fileName, baseFileName, append);
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void SATBoxBlur(const std::vector<uint32>& SAT, int width, int height, int radius, const char* baseFileName)
{
	std::vector<uint8> result;
	result.resize(SAT.size());

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			int startX = std::max(ix - radius - 1, -1);
			int startY = std::max(iy - radius - 1, -1);

			int endX = std::min(ix + radius, width - 1);
			int endY = std::min(iy + radius, height - 1);

			uint32 A = (startX >= 0 && startY >= 0) ? SAT[startY*width + startX] : 0;
			uint32 B = (startY >= 0) ? SAT[startY*width + endX] : 0;
			uint32 C = (startX >= 0) ? SAT[endY*width + startX] : 0;
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
	stbi_write_png(fileName, width, height, 1, &result[0], width);
}

void AATBoxBlur(const std::vector<uint8>& AAT, const std::vector<uint32>& SAT, int width, int height, int radius, const char* baseFileName)
{
	// TODO: remove SAT parameter

	std::vector<uint8> result;
	result.resize(AAT.size());

	for (int iy = 0; iy < height; ++iy)
	{
		for (int ix = 0; ix < width; ++ix)
		{
			if (ix == 5 && iy == 5)
			{
				int ijkl = 0;
			}

			int startX = std::max(ix - radius - 1, -1);
			int startY = std::max(iy - radius - 1, -1);

			int endX = std::min(ix + radius, width - 1);
			int endY = std::min(iy + radius, height - 1);

			uint32 A = (startX >= 0 && startY >= 0) ? uint32(AAT[startY*width + startX]) * uint32((startY + 1)*(startX + 1)) : 0;
			uint32 B = (startY >= 0) ? uint32(AAT[startY*width + endX]) * uint32((startY + 1)*(endX + 1)) : 0;
			uint32 C = (startX >= 0) ? uint32(AAT[endY*width + startX]) * uint32((endY + 1)*(startX + 1)) : 0;
			uint32 D = uint32(AAT[endY*width + endX]) * uint32((endY + 1)*(endX + 1));

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

void TestAATvsSAT(uint8* source, int width, int height, const char* baseFileName)
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

	// TODO: temp
	int radiuses[] = { 0, 1, 5, 25 };//, 100 };

	for (size_t index = 0; index < _countof(radiuses); ++index)
	{
		// regular box blur
		BoxBlur(source, width, height, radiuses[index], baseFileName);

		// box blur with SAT
		SATBoxBlur(SAT, width, height, radiuses[index], baseFileName);

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
* the left and top of images (min side?) seem to be black on the sat images compared to regular box blurred images. investigate

* multithread? make a std::list of std::function's or something, and have a multithreaded process run through them.

* for AAT try not adding 0.5 to round?
 * try noise?
 * make sure numbers look good (they "do" so far but image is way bad)

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